#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>
#include <getopt.h>

// Перечисления для флагов, цветов и кодов ошибок
typedef enum {
    LS_ALL = 1,
    LS_LONG = 2
} ls_flags_t;

typedef enum {
    COLOR_FILE,
    COLOR_DIR,
    COLOR_EXEC,
    COLOR_LINK
} file_color_t;

typedef enum {
    ERR_NOT_ENOUGH_ARGS,
    ERR_INVALID_OPTION,
    ERR_FILE_NOT_SPECIFIED,
    ERR_OPEN_DIR,
    ERR_READ_DIR,
    ERR_STAT,
    ERR_GET_USER,
    ERR_GET_GROUP
} error_code_t;

static const int COLOR_CODES[] = {39, 34, 32, 36}; // Белый, Синий, Зеленый, Бирюзовый
static const char * const VALID_OPTIONS = "hla";
static const char * const PERMISSION_CHARS = "rwx";
static const int TIME_STRING_OFFSET = 4;
static const int TIME_STRING_LENGTH = 12;
static const int MAX_PATH_SIZE = 4096;

// ANSI escape последовательности
static const char * const ANSI_RESET = "\x1b[0m";
static const char * const ANSI_COLOR_PREFIX = "\x1b[;";

// Глобальные переменные
static const char *base_path = NULL;
static char *path_buffer = NULL;
static DIR *open_dir = NULL;
static size_t entry_count = 0;
static struct dirent **entries = NULL;
static int flags = 0;

// Прототипы функций
static void init_program(void);
static void cleanup_and_exit(error_code_t error);
static void compose_path(const char *filename);
static void collect_entries(void);
static void print_entry(struct dirent *entry);
static void list_directory(const char *dir_path);
static void close_dir_on_exit(void);
static void free_path_on_exit(void);
static void free_entries_on_exit(void);
static int compare_entries(const void *a, const void *b);

int main(int argc, char **argv)
{
    init_program();

    // Парсинг опций
    opterr = 0; // Отключить сообщения об ошибках getopt
    int option;
    while ((option = getopt(argc, argv, VALID_OPTIONS)) != -1) {
        switch (option) {
            case 'h':
                printf("ls - список содержимого директории\n"
                       "использование: ls [параметры...] [файл]\n"
                       " -a - не игнорировать записи, начинающиеся с '.'\n"
                       " -l - использовать длинный формат списка\n"
                       " -h - показать это сообщение\n");
                exit(EXIT_SUCCESS);
            case 'l':
                flags |= LS_LONG;
                break;
            case 'a':
                flags |= LS_ALL;
                break;
            default:
                cleanup_and_exit(ERR_INVALID_OPTION);
        }
    }

    // Определение целевой директории
    const char *target_dir = (optind == argc) ? "." : argv[optind];
    list_directory(target_dir);
    exit(EXIT_SUCCESS);
}

static void init_program(void)
{
    path_buffer = malloc(MAX_PATH_SIZE);
    if (!path_buffer) {
        fprintf(stderr, "[ls]: Ошибка выделения памяти\n");
        exit(EXIT_FAILURE);
    }
    atexit(free_path_on_exit);
}

static void cleanup_and_exit(error_code_t error)
{
    switch (error) {
        case ERR_NOT_ENOUGH_ARGS:
            fprintf(stderr, "[ls]: Недостаточно аргументов! Использование: ls -h\n");
            break;
        case ERR_INVALID_OPTION:
            fprintf(stderr, "[ls]: Ошибка: неверная опция, см. \"ls -h\"\n");
            break;
        case ERR_FILE_NOT_SPECIFIED:
            fprintf(stderr, "[ls]: Ошибка! Файл не указан\n");
            break;
        case ERR_OPEN_DIR:
            fprintf(stderr, "[ls]: Ошибка при открытии директории! %s\n", strerror(errno));
            break;
        case ERR_READ_DIR:
            fprintf(stderr, "[ls]: Ошибка при чтении файлов! %s\n", strerror(errno));
            break;
        case ERR_STAT:
            fprintf(stderr, "[ls]: Ошибка при получении информации о файле! %s\n", strerror(errno));
            break;
        case ERR_GET_USER:
            fprintf(stderr, "[ls]: Ошибка при получении имени пользователя! %s\n", strerror(errno));
            break;
        case ERR_GET_GROUP:
            fprintf(stderr, "[ls]: Ошибка при получении имени группы! %s\n", strerror(errno));
            break;
    }
    exit(EXIT_FAILURE);
}

// Построение полного пути из директории и имени файла
static void compose_path(const char *filename)
{
    if (base_path == NULL) {
        base_path = ".";
    }
    int written = snprintf(path_buffer, MAX_PATH_SIZE, "%s/%s", base_path, filename);
    if (written < 0 || (size_t)written >= MAX_PATH_SIZE) {
        fprintf(stderr, "[ls]: Путь слишком длинный\n");
        exit(EXIT_FAILURE);
    }
}

// Сбор и сортировка записей директории
static void collect_entries(void)
{
    errno = 0;
    size_t total_blocks = 0;
    
    // Первый проход: подсчет записей и блоков
    for (struct dirent *cur_file; (cur_file = readdir(open_dir)) != NULL; entry_count++) {
        if ((flags & LS_ALL) || cur_file->d_name[0] != '.') {
            struct stat temp;
            compose_path(cur_file->d_name);
            if (lstat(path_buffer, &temp) == 0) {
                total_blocks += temp.st_blocks;
            }
        }
    }
    
    if (errno) {
        cleanup_and_exit(ERR_READ_DIR);
    }
    
    entries = malloc(entry_count * sizeof(struct dirent *));
    if (!entries) {
        fprintf(stderr, "[ls]: Ошибка выделения памяти\n");
        exit(EXIT_FAILURE);
    }
    atexit(free_entries_on_exit);

    rewinddir(open_dir);
    int i = 0;
    for (struct dirent *cur_file; (cur_file = readdir(open_dir)) != NULL; entries[i++] = cur_file);
    
    qsort(entries, entry_count, sizeof(struct dirent *), compare_entries);
    
    // Вывод total для длинного формата
    if (flags & LS_LONG) {
        printf("total %zu\n", total_blocks / 2);
    }
}

// Вывод одной записи директории
static void print_entry(struct dirent *entry)
{
    if (entry->d_name[0] == '.' && !(flags & LS_ALL)) {
        return;
    }
    
    compose_path(entry->d_name);
    file_color_t filename_color = COLOR_FILE;
    struct stat file_info;
    
    if (lstat(path_buffer, &file_info) == -1) {
        cleanup_and_exit(ERR_STAT);
    }

    if (flags & LS_LONG) {
        // Определение типа файла и установка цвета
        char file_type_char = '?';
        if (S_ISREG(file_info.st_mode)) {
            file_type_char = '-';
            if (file_info.st_mode & S_IXUSR) {
                filename_color = COLOR_EXEC;
            }
        } else if (S_ISDIR(file_info.st_mode)) {
            file_type_char = 'd';
            filename_color = COLOR_DIR;
        } else if (S_ISBLK(file_info.st_mode)) {
            file_type_char = 'b';
        } else if (S_ISLNK(file_info.st_mode)) {
            file_type_char = 'l';
            filename_color = COLOR_LINK;
        }
        putchar(file_type_char);

        // Права доступа к файлу
        int i = 0;
        for (uint64_t mask = S_IRUSR; mask > 0; mask >>= 1, i++) {
            putchar(file_info.st_mode & mask ? PERMISSION_CHARS[i % 3] : '-');
        }
        putchar(' ');

        // Получение имени пользователя
        errno = 0;
        struct passwd *pwd_file = getpwuid(file_info.st_uid);
        if (pwd_file == NULL && errno) {
            cleanup_and_exit(ERR_GET_USER);
        }

        // Получение имени группы
        errno = 0;
        struct group *grp_file = getgrgid(file_info.st_gid);
        if (grp_file == NULL && errno) {
            cleanup_and_exit(ERR_GET_GROUP);
        }

        // Количество ссылок, пользователь/uid, группа/gid, размер
        printf("%lu ", file_info.st_nlink);
        if (pwd_file) {
            printf("%-8s ", pwd_file->pw_name);
        } else {
            printf("%-8lu ", (unsigned long)file_info.st_uid);
        }
        if (grp_file) {
            printf("%-8s ", grp_file->gr_name);
        } else {
            printf("%-8lu ", (unsigned long)file_info.st_gid);
        }
        printf("%lu ", (unsigned long)file_info.st_size);

        // Время модификации
        char *time_str = ctime(&file_info.st_mtime);
        char output_time_str[TIME_STRING_LENGTH + 1];
        strncpy(output_time_str, time_str + TIME_STRING_OFFSET, TIME_STRING_LENGTH);
        output_time_str[TIME_STRING_LENGTH] = '\0';
        printf("%s ", output_time_str);

        // Имя файла с цветом
        if (strchr(entry->d_name, ' ') != NULL) {
            printf("%s%dm`%s`%s", ANSI_COLOR_PREFIX, COLOR_CODES[filename_color],
                   entry->d_name, ANSI_RESET);
        } else {
            printf("%s%dm%s%s", ANSI_COLOR_PREFIX, COLOR_CODES[filename_color],
                   entry->d_name, ANSI_RESET);
        }

        // Для символических ссылок показать цель
        if (S_ISLNK(file_info.st_mode)) {
            size_t bufsize = (file_info.st_size > 0) ? file_info.st_size + 1 : PATH_MAX;
            char *buf = malloc(bufsize);
            if (!buf) {
                cleanup_and_exit(ERR_STAT);
            }
            int len = readlink(path_buffer, buf, bufsize - 1);
            if (len == -1) {
                free(buf);
                cleanup_and_exit(ERR_STAT);
            }
            buf[len] = '\0';
            printf(" -> %s", buf);
            free(buf);
        }
        putchar('\n');
    } else {
        // Короткий формат
        if (S_ISREG(file_info.st_mode) && (file_info.st_mode & S_IXUSR)) {
            filename_color = COLOR_EXEC;
        } else if (S_ISDIR(file_info.st_mode)) {
            filename_color = COLOR_DIR;
        } else if (S_ISLNK(file_info.st_mode)) {
            filename_color = COLOR_LINK;
        }

        // Имя файла с цветом
        if (strchr(entry->d_name, ' ') != NULL) {
            printf("%s%dm`%s`%s  ", ANSI_COLOR_PREFIX, COLOR_CODES[filename_color], 
                   entry->d_name, ANSI_RESET);
        } else {
            printf("%s%dm%s%s  ", ANSI_COLOR_PREFIX, COLOR_CODES[filename_color], 
                   entry->d_name, ANSI_RESET);
        }
    }
}

static void list_directory(const char *dir_path)
{
    open_dir = opendir(dir_path);
    if (open_dir == NULL) {
        cleanup_and_exit(ERR_OPEN_DIR);
    }

    atexit(close_dir_on_exit);
    base_path = dir_path;
    
    errno = 0;
    collect_entries();
    
    for (size_t i = 0; i < entry_count; ++i) {
        print_entry(entries[i]);
    }
    
    if (!(flags & LS_LONG)) {
        putchar('\n');
    }
}

// Функции очистки ресурсов
static void close_dir_on_exit(void) 
{ 
    if (open_dir) { 
        closedir(open_dir); 
        open_dir = NULL; 
    } 
}

static void free_path_on_exit(void) 
{ 
    free(path_buffer); 
}

static void free_entries_on_exit(void) 
{ 
    free(entries); 
}

static int compare_entries(const void *a, const void *b)
{
    struct dirent *first = *(struct dirent **)a;
    struct dirent *second = *(struct dirent **)b;
    return strcmp(first->d_name, second->d_name);
}