#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <getopt.h>
#include <errno.h>

struct file_header {
    char name[1024];
    struct stat metadata;
    char is_deleted;
};

#define MAX_FILE_SIZE (1024LL * 1024LL * 1024LL)

/* ===== Вспомогательные функции (рефакторинг, без изменения логики) ===== */

/*
 * Полностью записывает переданный буфер в дескриптор файла.
 * Возвращает 0 при успехе, -1 при ошибке (errno выставлен системой).
 */
static int write_all(int fd, const void *buffer, size_t length) {
    const char *ptr = (const char *)buffer;
    size_t remaining = length;
    while (remaining > 0) {
        ssize_t written = write(fd, ptr, remaining);
        if (written < 0) {
            return -1;
        }
        if (written == 0) {
            errno = EIO;
            return -1;
        }
        ptr += written;
        remaining -= (size_t)written;
    }
    return 0;
}

/*
 * Копирует строго length байт из in_fd в out_fd, чтением/записью порциями.
 * Возвращает 0 при успехе, -1 при ошибке.
 */
static int copy_bytes(int in_fd, int out_fd, off_t length) {
    char buf[4096];
    off_t remaining = length;
    while (remaining > 0) {
        ssize_t to_read = (remaining > (off_t)sizeof(buf)) ? (ssize_t)sizeof(buf) : (ssize_t)remaining;
        ssize_t br = read(in_fd, buf, to_read);
        if (br <= 0) {
            return -1; /* читать ошибку будет вызывающая сторона */
        }
        if (write_all(out_fd, buf, (size_t)br) == -1) {
            return -1;
        }
        remaining -= br;
    }
    return 0;
}

/*
 * Пропускает в потоке/файле length байт (через lseek или последовательное чтение).
 * Возвращает 0 при успехе, -1 при ошибке.
 */
static int skip_bytes(int fd, off_t length) {
    if (length <= 0) return 0;
    if (lseek(fd, length, SEEK_CUR) != -1) return 0;
    /* если lseek недоступен (например, поток), читаем и выбрасываем */
    char buf[4096];
    off_t remaining = length;
    while (remaining > 0) {
        ssize_t to_read = (remaining > (off_t)sizeof(buf)) ? (ssize_t)sizeof(buf) : (ssize_t)remaining;
        ssize_t br = read(fd, buf, to_read);
        if (br <= 0) return -1;
        remaining -= br;
    }
    return 0;
}

/*
 * Восстанавливает права доступа, владельца и времена из структуры stat для файла path.
 */
static void restore_metadata_from_stat(const char *path, const struct stat *st) {
    if (!path || !st) return;
    if (chmod(path, st->st_mode) == -1) {
        perror("Предупреждение: не удалось восстановить права доступа");
    }
    /* chown может не сработать без root, это не критично */
    (void)chown(path, st->st_uid, st->st_gid);
    struct utimbuf times = { st->st_atime, st->st_mtime };
    if (utime(path, &times) == -1) {
        perror("Предупреждение: не удалось восстановить время модификации");
    }
}

/*
 * Выводит краткую справку по использованию утилиты и ключам.
 */
void print_help() {
    printf("Использование: ./archiver <имя_архива> [опции] [файл]\n");
    printf("Опции:\n");
    printf("  -i, --input <file>    Добавить файл в архив\n");
    printf("  -e, --extract <file>  Извлечь файл из архива (с удалением записи)\n");
    printf("  -s, --stat            Показать содержимое архива\n");
    printf("  -h, --help            Показать эту справку\n");
}

/*
 * Сжимает архив: копирует только актуальные записи (is_deleted == 0)
 * во временный файл и атомарно заменяет им исходный архив.
 * Возвращает 0 при успехе, -1 при ошибке.
 */
int compact_archive(const char *archive_name) {
    int in_fd = open(archive_name, O_RDONLY);
    if (in_fd == -1) {
        perror("compact: не удалось открыть архив для чтения");
        return -1;
    }

    /* temp template: "<archive_name>.tmpXXXXXX" */
    size_t tlen = strlen(archive_name) + 12;
    char *tmp_name = malloc(tlen);
    if (!tmp_name) { close(in_fd); return -1; }
    snprintf(tmp_name, tlen, "%s.tmpXXXXXX", archive_name);

    int tmp_fd = mkstemp(tmp_name);
    if (tmp_fd == -1) {
        perror("compact: не удалось создать временный файл");
        free(tmp_name);
        close(in_fd);
        return -1;
    }

    /* попытаться применить права исходного файла к временному */
    struct stat arch_st;
    if (fstat(in_fd, &arch_st) == 0) {
        fchmod(tmp_fd, arch_st.st_mode);
    }

    struct file_header header;
    ssize_t r;

    while ((r = read(in_fd, &header, sizeof(header))) == sizeof(header)) {
        off_t remaining = header.metadata.st_size;

        if (!header.is_deleted) {
            /* Переписать заголовок */
            if (write_all(tmp_fd, &header, sizeof(header)) == -1) {
                perror("compact: ошибка записи заголовка во временный файл");
                close(in_fd);
                close(tmp_fd);
                unlink(tmp_name);
                free(tmp_name);
                return -1;
            }

            if (copy_bytes(in_fd, tmp_fd, remaining) == -1) {
                perror("compact: ошибка копирования данных");
                close(in_fd);
                close(tmp_fd);
                unlink(tmp_name);
                free(tmp_name);
                return -1;
            }
        } else {
            /* Пропустить данные удалённой записи */
            if (skip_bytes(in_fd, header.metadata.st_size) == -1) {
                perror("compact: ошибка пропуска данных");
                close(in_fd);
                close(tmp_fd);
                unlink(tmp_name);
                free(tmp_name);
                return -1;
            }
        }
    }

    if (r == -1) {
        perror("compact: ошибка чтения архива");
        close(in_fd);
        close(tmp_fd);
        unlink(tmp_name);
        free(tmp_name);
        return -1;
    }

    /* flush & close */
    fsync(tmp_fd);
    close(tmp_fd);
    close(in_fd);

    /* заменить оригинал временным файлом */
    if (rename(tmp_name, archive_name) == -1) {
        perror("compact: не удалось заменить архив временным файлом");
        unlink(tmp_name);
        free(tmp_name);
        return -1;
    }

    free(tmp_name);
    return 0;
}

/*
 * Добавляет файл file_name в архив archive_name: записывает заголовок и данные.
 */
void archive_file(const char *archive_name, const char *file_name) {
    int arch_fd = open(archive_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (arch_fd == -1) {
        perror("Ошибка: не удалось открыть или создать архив");
        return;
    }

    int in_fd = open(file_name, O_RDONLY);
    if (in_fd == -1) {
        perror("Ошибка: не удалось открыть входной файл");
        close(arch_fd);
        return;
    }

    struct file_header header;

    if (strlen(file_name) >= sizeof(header.name)) {
        printf("Ошибка: имя файла '%s' слишком длинное (максимум %zu символов)\n",
               file_name, sizeof(header.name) - 1);
        close(in_fd);
        close(arch_fd);
        return;
    }
    memset(&header, 0, sizeof(header));
    strncpy(header.name, file_name, sizeof(header.name) - 1);

    if (fstat(in_fd, &header.metadata) == -1) {
        perror("Ошибка: не удалось получить метаданные файла");
        close(in_fd);
        close(arch_fd);
        return;
    }
    header.is_deleted = 0;

    if (write_all(arch_fd, &header, sizeof(header)) == -1) {
        perror("Ошибка: запись заголовка в архив не удалась");
        close(in_fd);
        close(arch_fd);
        return;
    }

    if (copy_bytes(in_fd, arch_fd, header.metadata.st_size) == -1) {
        perror("Ошибка: добавление данных файла в архив не удалось");
        close(in_fd);
        close(arch_fd);
        return;
    }

    printf("Готово: файл '%s' добавлен в архив '%s'.\n", file_name, archive_name);

    close(in_fd);
    close(arch_fd);
}

/*
 * Извлекает первый найденный файл по имени из архива и помечает его запись удалённой.
 * После пометки выполняет компактацию архива.
 */
void extract_file(const char *archive_name, const char *file_name) {
    int arch_fd = open(archive_name, O_RDWR);
    if (arch_fd == -1) {
        perror("Ошибка: не удалось открыть архив");
        return;
    }

    struct file_header header;
    ssize_t bytes_read;
    int found = 0;

    while (1) {
        off_t header_start = lseek(arch_fd, 0, SEEK_CUR);
        if (header_start == -1) {
            perror("Ошибка определения позиции в архиве");
            break;
        }

        bytes_read = read(arch_fd, &header, sizeof(header));
        if (bytes_read == 0) break; /* EOF */
        if (bytes_read != sizeof(header)) {
            perror("Ошибка: чтение заголовка из архива не удалось");
            break;
        }

        /* если совпадает имя и не помечен как удалённый — извлекаем */
        if (strcmp(header.name, file_name) == 0 && !header.is_deleted) {
            found = 1;

            if (header.metadata.st_size > MAX_FILE_SIZE) {
                printf("Предупреждение: файл '%s' слишком большой для извлечения (размер: %lld байт)\n",
                       file_name, (long long)header.metadata.st_size);
                /* пропустим данные и выйдем */
                if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
                    perror("Ошибка: пропуск данных большого файла не удался");
                }
                break;
            }

            int out_fd = open(header.name, O_WRONLY | O_CREAT | O_TRUNC, header.metadata.st_mode);
            if (out_fd == -1) {
                perror("Ошибка: не удалось создать файл для извлечения");
                /* пропустим данные */
                if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
                    perror("Ошибка: не удалось пропустить данные при неудачном создании файла");
                }
                break;
            }

            if (copy_bytes(arch_fd, out_fd, header.metadata.st_size) == -1) {
                perror("Ошибка: извлечение данных файла не удалось");
                close(out_fd);
                break;
            }

            close(out_fd);

            /* Восстановить атрибуты */
            restore_metadata_from_stat(header.name, &header.metadata);

            /* Пометить запись как удалённую в архиве */
            header.is_deleted = 1;
            if (lseek(arch_fd, header_start, SEEK_SET) == -1) {
                perror("Ошибка: позиционирование в архиве для пометки удаления не удалось");
            } else if (write(arch_fd, &header, sizeof(header)) != sizeof(header)) {
                perror("Ошибка: запись пометки удаления в архив не удалась");
            }

            /* Закрываем дескриптор и запускаем компактацию архива */
            close(arch_fd);
            if (compact_archive(archive_name) == -1) {
                fprintf(stderr, "Предупреждение: сжатие архива после удаления не выполнено. Запись помечена, но размер может не уменьшиться.\n");
            }

            printf("Готово: файл '%s' извлечён и удалён из архива.\n", file_name);
            return;
        } else {
            /* Пропустить данные этой записи и продолжить */
            if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
                perror("Ошибка: позиционирование в архиве при пропуске записи не удалось");
                break;
            }
        }
    }

    if (!found) {
        printf("Инфо: файл '%s' не найден в архиве.\n", file_name);
    }

    close(arch_fd);
}

/*
 * Печатает список актуальных файлов в архиве с размерами и временем модификации.
 */
void show_stat(const char *archive_name) {
    int arch_fd = open(archive_name, O_RDONLY);
    if (arch_fd == -1) {
        perror("Ошибка: не удалось открыть архив");
        return;
    }

    struct file_header header;
    ssize_t bytes_read;
    printf("Содержимое архива '%s' (помеченные как удалённые скрыты):\n", archive_name);
    printf("--------------------------------------------------\n");
    printf("%-30s %-12s %-20s\n", "Имя файла", "Размер (байт)", "Дата изменения");
    printf("--------------------------------------------------\n");

    while ((bytes_read = read(arch_fd, &header, sizeof(header))) > 0) {
        if (bytes_read != sizeof(header)) {
            perror("Ошибка: чтение заголовка при просмотре архива не удалось");
            break;
        }
        if (!header.is_deleted) {
            char time_buf[80];
            struct tm *tm = localtime(&header.metadata.st_mtime);
            if (tm) strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm);
            else strncpy(time_buf, "unknown", sizeof(time_buf));
            printf("%-30s %-10lld %-20s\n", header.name, (long long)header.metadata.st_size, time_buf);
        }
        if (lseek(arch_fd, header.metadata.st_size, SEEK_CUR) == -1) {
            perror("Ошибка: позиционирование в архиве при просмотре не удалось");
            break;
        }
    }

    close(arch_fd);
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }

    if (argc < 3) {
        print_help();
        return 1;
    }

    const char *archive_name = argv[1];

    static struct option long_options[] = {
        {"input",   required_argument, 0, 'i'},
        {"extract", required_argument, 0, 'e'},
        {"stat",    no_argument,       0, 's'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 2;
    int opt;
    int option_index = 0;
    opt = getopt_long(argc, argv, "i:e:sh", long_options, &option_index);

    switch (opt) {
        case 'i':
            if (optarg) archive_file(archive_name, optarg);
            break;
        case 'e':
            if (optarg) extract_file(archive_name, optarg);
            break;
        case 's':
            show_stat(archive_name);
            break;
        case 'h':
            print_help();
            break;
        default:
            print_help();
            return 1;
    }

    return 0;
}