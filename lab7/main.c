#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/types.h>
#include <inttypes.h>

// Имя объекта разделяемой памяти
#define SHM_NAME        "/ipc_shm_example_v1"
// Путь к файлу блокировки для предотвращения запуска нескольких writer'ов
#define LOCKFILE_PATH   "/tmp/ipc_writer.lock"
// Максимальная длина сообщения
#define MSG_MAX         256
// Размер структуры разделяемой памяти
#define SHM_SIZE        (sizeof(struct shared_area))

// Структура данных в разделяемой памяти
struct shared_area {
    uint64_t seq;           // Счетчик последовательности сообщений
    pid_t sender_pid;       // PID процесса-отправителя
    time_t tsec;            // Секунды времени отправки
    long tnsec;             // Наносекунды времени отправки
    char message[MSG_MAX];  // Текст сообщения
};

// Глобальные переменные для управления ресурсами
static int g_shm_fd = -1;                    // Дескриптор разделяемой памяти
static struct shared_area *g_shm_ptr = NULL; // Указатель на отображенную память
static int g_lockfd = -1;                    // Дескриптор файла блокировки
static bool g_is_writer = false;              // Флаг: является ли процесс writer'ом

// Прототипы функций
static void cleanup_resources(int code);
static void signal_handler(int sig);

/**
 * Задержка выполнения на указанное количество микросекунд
 * @param microseconds - количество микросекунд для задержки
 */
static void delay_microseconds(long microseconds) {
    struct timespec req;
    req.tv_sec = microseconds / 1000000L;
    req.tv_nsec = (microseconds % 1000000L) * 1000L;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        // Повторяем при прерывании сигналом
    }
}

/**
 * Форматирует время из секунд и наносекунд в строку формата "YYYY-MM-DD HH:MM:SS.mmm"
 * @param sec - секунды с начала эпохи
 * @param nsec - наносекунды
 * @param buf - буфер для результата
 * @param bufsz - размер буфера
 */
static void format_timestamp(time_t sec, long nsec, char *buf, size_t bufsz) {
    struct tm tm;
    if (localtime_r(&sec, &tm) == NULL) {
        snprintf(buf, bufsz, "unknown-time");
        return;
    }

    size_t used = strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);

    if (used >= bufsz) {
        if (bufsz > 0) buf[bufsz - 1] = '\0';
        return;
    }

    long msec = (long)(nsec / 1000000L);
    snprintf(buf + used, bufsz - used, ".%03ld", msec);
}

/**
 * Обработчик сигналов SIGINT и SIGTERM
 * Выполняет корректное завершение работы программы
 * @param sig - номер сигнала
 */
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (g_is_writer) {
            printf("\nWriter received signal, cleaning up and exiting...\n");
        } else {
            printf("\nReader received signal, exiting...\n");
        }
        cleanup_resources(0);
    }
}

/**
 * Очистка всех ресурсов и завершение программы
 * Освобождает разделяемую память, закрывает файловые дескрипторы,
 * удаляет объекты разделяемой памяти и файлы блокировки
 * @param code - код возврата для exit()
 */
static void cleanup_resources(int code) {
    // Отключаем отображение разделяемой памяти
    if (g_shm_ptr && g_shm_ptr != MAP_FAILED) {
        munmap((void*)g_shm_ptr, SHM_SIZE);
        g_shm_ptr = NULL;
    }

    // Закрываем дескриптор разделяемой памяти
    if (g_shm_fd >= 0) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }
    
    // Если мы writer, удаляем объект разделяемой памяти
    if (g_is_writer) {
        if (shm_unlink(SHM_NAME) != 0) {
            if (errno != ENOENT) {
                fprintf(stderr, "Warning: shm_unlink(%s) failed: %s\n", SHM_NAME, strerror(errno));
            }
        } else {
            printf("Writer: shm_unlink(%s) succeeded.\n", SHM_NAME);
        }
    }

    // Обработка файла блокировки
    if (g_lockfd >= 0) {
        pid_t file_pid = -1;
        bool pid_read = false;

        // Читаем PID из файла блокировки
        (void)lseek(g_lockfd, 0, SEEK_SET);
        char pidbuf[64] = {0};
        ssize_t r = read(g_lockfd, pidbuf, sizeof(pidbuf) - 1);
        if (r > 0) {
            pidbuf[r] = '\0';
            char *endptr = NULL;
            long v = strtol(pidbuf, &endptr, 10);
            if (endptr != pidbuf && v > 0 && v <= 2147483647) {
                file_pid = (pid_t)v;
                pid_read = true;
            }
        }
        
        // Удаляем файл блокировки только если мы writer и PID совпадает
        if (g_is_writer && (!pid_read || file_pid == getpid())) {
            if (unlink(LOCKFILE_PATH) != 0) {
                if (errno != ENOENT) {
                    fprintf(stderr, "Warning: unlink(%s) failed: %s\n", LOCKFILE_PATH, strerror(errno));
                }
            } else {
                printf("Writer: removed lockfile %s\n", LOCKFILE_PATH);
            }
        } else if (g_is_writer) {
            fprintf(stderr, "Note: not removing %s (owner pid mismatch or unreadable).\n", LOCKFILE_PATH);
        }

        // Снимаем блокировку и закрываем файл
        if (flock(g_lockfd, LOCK_UN) != 0) {
            fprintf(stderr, "Warning: flock(LOCK_UN) failed: %s\n", strerror(errno));
        }
        close(g_lockfd);
        g_lockfd = -1;
    }

    exit(code);
}

/**
 * Основная функция reader'а
 * Читает сообщения из разделяемой памяти и выводит их на экран
 * @return код возврата (0 при успехе, >0 при ошибке)
 */
static int reader_main(void) {
    // Ожидаем появления разделяемой памяти (writer может еще не запуститься)
    int attempts = 0;
    while (1) {
        g_shm_fd = shm_open(SHM_NAME, O_RDONLY, 0);
        if (g_shm_fd >= 0) break;
        if (errno == ENOENT) {
            if (attempts == 0) {
                fprintf(stderr, "Reader: shared memory %s not found yet. Waiting for writer to start...\n", SHM_NAME);
            }
            attempts++;
            delay_microseconds(200000); // Ждем 200 мс перед повторной попыткой
            continue;
        } else {
            perror("shm_open (reader) failed");
            return 10;
        }
    }

    // Отображаем разделяемую память в адресное пространство процесса
    g_shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, g_shm_fd, 0);
    if (g_shm_ptr == MAP_FAILED) {
        perror("mmap (reader) failed");
        return 11;
    }

    // Устанавливаем обработчики сигналов для корректного завершения
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Reader started (pid=%d). Attached to shared memory %s\n", (int)getpid(), SHM_NAME);

    // Запоминаем текущий номер последовательности
    uint64_t last_seq = __atomic_load_n(&g_shm_ptr->seq, __ATOMIC_SEQ_CST);

    // Основной цикл чтения сообщений
    while (1) {
        uint64_t cur_seq = __atomic_load_n(&g_shm_ptr->seq, __ATOMIC_SEQ_CST);
        
        // Если номер последовательности изменился, значит появилось новое сообщение
        if (cur_seq != last_seq) {
            // Атомарно читаем данные из разделяемой памяти
            pid_t sender_pid = __atomic_load_n(&g_shm_ptr->sender_pid, __ATOMIC_SEQ_CST);
            time_t tsec = __atomic_load_n(&g_shm_ptr->tsec, __ATOMIC_SEQ_CST);
            long tnsec = __atomic_load_n(&g_shm_ptr->tnsec, __ATOMIC_SEQ_CST);
            char message[MSG_MAX];

            // Копируем сообщение (не атомарно, но это безопасно, так как writer не изменяет его после изменения seq)
            memcpy(message, (const void*)g_shm_ptr->message, MSG_MAX);
            message[MSG_MAX-1] = '\0';

            // Получаем текущее время для сравнения
            struct timespec rts;
            if (clock_gettime(CLOCK_REALTIME, &rts) != 0) {
                rts.tv_sec = 0;
                rts.tv_nsec = 0;
            }
            char our_time[64], sender_time[64];
            format_timestamp(rts.tv_sec, rts.tv_nsec, our_time, sizeof(our_time));
            format_timestamp(tsec, tnsec, sender_time, sizeof(sender_time));

            // Выводим информацию о полученном сообщении
            printf("R(pid=%d) local=%s | received seq=%" PRIu64 " from pid=%d at=%s -> \"%s\"\n",
                   (int)getpid(), our_time, cur_seq, (int)sender_pid, sender_time, message);

            last_seq = cur_seq;
        }

        // Небольшая задержка перед следующей проверкой (100 мс)
        delay_microseconds(100000);
    }

    return 0;
}

/**
 * Основная функция writer'а
 * Создает разделяемую память и периодически записывает в нее сообщения
 * @return код возврата (0 при успехе, >0 при ошибке)
 */
static int writer_main(void) {
    // Создаем и блокируем файл блокировки для предотвращения запуска нескольких writer'ов
    g_lockfd = open(LOCKFILE_PATH, O_CREAT | O_RDWR, 0600);
    if (g_lockfd < 0) {
        perror("open(lockfile) failed");
        return 1;
    }
    
    // Пытаемся получить эксклюзивную блокировку (неблокирующая)
    if (flock(g_lockfd, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "Another writer is already running (could not acquire lock on %s). Exiting.\n", LOCKFILE_PATH);
        close(g_lockfd);
        g_lockfd = -1;
        return 2;
    }

    // Записываем свой PID в файл блокировки
    {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
        if (len < 0) len = 0;
        (void)ftruncate(g_lockfd, 0);
        (void)lseek(g_lockfd, 0, SEEK_SET);
        ssize_t wr = write(g_lockfd, buf, (size_t)len);
        (void)wr;
        fsync(g_lockfd);
    }

    // Создаем или открываем объект разделяемой памяти
    g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (g_shm_fd < 0) {
        perror("shm_open (writer) failed");
        cleanup_resources(3);
        return 3;
    }
    
    // Устанавливаем размер разделяемой памяти
    if (ftruncate(g_shm_fd, SHM_SIZE) != 0) {
        perror("ftruncate (writer) failed");
        cleanup_resources(4);
        return 4;
    }
    
    // Отображаем разделяемую память в адресное пространство процесса
    g_shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shm_ptr == MAP_FAILED) {
        perror("mmap (writer) failed");
        cleanup_resources(5);
        return 5;
    }

    // Инициализируем разделяемую память нулевыми значениями
    __atomic_store_n(&g_shm_ptr->seq, (uint64_t)0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_shm_ptr->sender_pid, (pid_t)getpid(), __ATOMIC_SEQ_CST);
    g_shm_ptr->tsec = 0;
    g_shm_ptr->tnsec = 0;
    g_shm_ptr->message[0] = '\0';

    g_is_writer = true;
    
    // Устанавливаем обработчики сигналов для корректного завершения
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Writer started (pid=%d). Shared memory name: %s\n", (int)getpid(), SHM_NAME);
    printf("Press Ctrl-C to stop writer and unlink shared memory.\n");

    uint64_t seq_local = 0;
    
    // Основной цикл записи сообщений
    while (1) {
        // Получаем текущее время
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            perror("clock_gettime");
            sleep(1);
            continue;
        }

        // Формируем сообщение с текущим временем
        char msg[MSG_MAX];
        char timestr[64];
        format_timestamp(ts.tv_sec, ts.tv_nsec, timestr, sizeof(timestr));
        int n = snprintf(msg, sizeof(msg), "From pid=%d at %s", (int)getpid(), timestr);
        if (n < 0) {
            strncpy(msg, "format-error", sizeof(msg));
            msg[sizeof(msg)-1] = '\0';
        }

        // Атомарно записываем данные в разделяемую память
        __atomic_store_n(&g_shm_ptr->sender_pid, (pid_t)getpid(), __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_shm_ptr->tsec, (time_t)ts.tv_sec, __ATOMIC_SEQ_CST);
        __atomic_store_n(&g_shm_ptr->tnsec, (long)ts.tv_nsec, __ATOMIC_SEQ_CST);

        // Копируем сообщение (последнее действие перед увеличением seq)
        strncpy(g_shm_ptr->message, msg, MSG_MAX - 1);
        g_shm_ptr->message[MSG_MAX - 1] = '\0';

        // Увеличиваем счетчик последовательности (это сигнализирует reader'ам о новом сообщении)
        seq_local = __atomic_add_fetch(&g_shm_ptr->seq, (uint64_t)1, __ATOMIC_SEQ_CST);

        printf("W: seq=%" PRIu64 " wrote: %s\n", seq_local, g_shm_ptr->message);

        // Ждем 1 секунду перед следующей записью
        sleep(1);
    }

    return 0;
}

/**
 * Точка входа в программу
 * Определяет режим работы (writer или reader) и запускает соответствующую функцию
 * @param argc - количество аргументов командной строки
 * @param argv - массив аргументов командной строки
 * @return код возврата
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s writer|reader\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "writer") == 0) {
        return writer_main();
    } else if (strcmp(argv[1], "reader") == 0) {
        return reader_main();
    } else {
        fprintf(stderr, "Unknown mode '%s'. Use 'writer' or 'reader'.\n", argv[1]);
        return 1;
    }
}