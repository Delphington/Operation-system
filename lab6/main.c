//#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

/* ========== Вспомогательные функции ========== */

/**
 * Форматирует время в строку формата "YYYY-MM-DD HH:MM:SS"
 * @param t - время в формате time_t
 * @param buf - буфер для записи отформатированной строки
 * @param bufsz - размер буфера
 */
static void format_time(time_t t, char *buf, size_t bufsz) {
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);
}

/**
 * Выводит сообщение об ошибке и завершает программу
 * @param msg - сообщение об ошибке
 */
static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/**
 * Создает путь к FIFO файлу на основе UID пользователя
 * @return указатель на статический буфер с путем
 */
static const char *make_fifo_path(void) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/demo_fifo_%d", (int)getuid());
    return path;
}

/**
 * Выводит справку по использованию программы
 * @param prog - имя программы
 */
static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s pipe\n"
            "  %s fifo-writer\n"
            "  %s fifo-reader\n\n"
            "Examples:\n"
            "  # pipe demo (single run):\n"
            "  %s pipe\n\n"
            "  # fifo demo (two separate processes):\n"
            "  # In terminal 1:\n"
            "  %s fifo-reader\n"
            "  # In terminal 2:\n"
            "  %s fifo-writer\n",
            prog, prog, prog, prog, prog, prog);
}

/* ========== Основные функции демонстрации ========== */

/**
 * Демонстрирует работу с pipe (именованным каналом между процессами)
 * Создает pipe, порождает дочерний процесс, передает данные от родителя к ребенку
 */
static void run_pipe_demo(void) {
    int fds[2];
    if (pipe(fds) == -1) die("pipe");

    pid_t pid = fork();
    if (pid == -1) die("fork");

    if (pid == 0) {
        // Дочерний процесс: читает данные из pipe
        close(fds[1]);  // Закрываем дескриптор записи

        char buf[512];
        ssize_t r = read(fds[0], buf, sizeof(buf)-1);
        if (r == -1) {
            perror("child: read");
            close(fds[0]);
            _exit(EXIT_FAILURE);
        }
        if (r == 0) {
            fprintf(stderr, "child: no data received on pipe\n");
            close(fds[0]);
            _exit(EXIT_FAILURE);
        }
        buf[r] = '\0';
        close(fds[0]);

        // Ждем 6 секунд перед выводом
        const unsigned int SLEEP_SECONDS = 6;
        sleep(SLEEP_SECONDS);

        time_t t_child = time(NULL);
        char timestr_child[64];
        format_time(t_child, timestr_child, sizeof(timestr_child));

        printf("=== PIPE DEMO (child) ===\n");
        printf("Child current time: %s\n", timestr_child);
        printf("Received from parent: %s\n", buf);
        fflush(stdout);
        _exit(EXIT_SUCCESS);
    } else {
        // Родительский процесс: записывает данные в pipe
        close(fds[0]);  // Закрываем дескриптор чтения

        time_t t_parent = time(NULL);
        char timestr_parent[64];
        format_time(t_parent, timestr_parent, sizeof(timestr_parent));

        char out[512];
        int written = snprintf(out, sizeof(out), "Parent time: %s; Parent pid: %d\n",
                               timestr_parent, (int)getpid());
        if (written < 0 || (size_t)written >= sizeof(out)) {
            fprintf(stderr, "parent: message formatting error\n");
            close(fds[1]);
            waitpid(pid, NULL, 0);
            exit(EXIT_FAILURE);
        }

        ssize_t w = write(fds[1], out, (size_t)written);
        if (w == -1) {
            perror("parent: write");
            close(fds[1]);
            waitpid(pid, NULL, 0);
            exit(EXIT_FAILURE);
        }
        close(fds[1]);

        // Ожидаем завершения дочернего процесса
        int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            // Дочерний процесс завершился нормально
        } else {
            fprintf(stderr, "child ended abnormally\n");
        }
    }
}

/**
 * Процесс-писатель для демонстрации FIFO
 * Создает FIFO, открывает его для записи и отправляет сообщение
 */
static void run_fifo_writer(void) {
    const char *fifo = make_fifo_path();

    // Создаем FIFO, если его еще нет
    if (mkfifo(fifo, 0666) == -1) {
        if (errno != EEXIST) die("mkfifo");
    }

    int fd = open(fifo, O_WRONLY);
    if (fd == -1) die("fifo writer: open O_WRONLY");

    time_t t_parent = time(NULL);
    char timestr_parent[64];
    format_time(t_parent, timestr_parent, sizeof(timestr_parent));

    char out[512];
    int written = snprintf(out, sizeof(out), "Parent time: %s; Parent pid: %d\n",
                           timestr_parent, (int)getpid());
    if (written < 0 || (size_t)written >= sizeof(out)) {
        fprintf(stderr, "fifo writer: message formatting error\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    ssize_t w = write(fd, out, (size_t)written);
    if (w == -1) {
        perror("fifo writer: write");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);

    printf("fifo-writer: wrote message and exited\n");
}

/**
 * Процесс-читатель для демонстрации FIFO
 * Создает FIFO, открывает его для чтения и получает сообщение
 */
static void run_fifo_reader(void) {
    const char *fifo = make_fifo_path();

    // Создаем FIFO, если его еще нет
    if (mkfifo(fifo, 0666) == -1) {
        if (errno != EEXIST) die("mkfifo (reader)");
    }

    int fd = open(fifo, O_RDONLY);
    if (fd == -1) die("fifo reader: open O_RDONLY");

    char buf[512];
    ssize_t r = read(fd, buf, sizeof(buf)-1);
    if (r == -1) {
        perror("fifo reader: read");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if (r == 0) {
        buf[0] = '\0';
        close(fd);
    } else {
        buf[r] = '\0';
        close(fd);
    }

    // Ждем 11 секунд перед выводом
    const unsigned int SLEEP_SECONDS = 11;
    sleep(SLEEP_SECONDS);

    time_t t_child = time(NULL);
    char timestr_child[64];
    format_time(t_child, timestr_child, sizeof(timestr_child));

    printf("=== FIFO DEMO (reader) ===\n");
    printf("Reader current time: %s\n", timestr_child);
    printf("Received from writer: %s\n", buf);
    fflush(stdout);

    // Удаляем FIFO файл после использования
    if (unlink(fifo) == -1) {
        if (errno != ENOENT) {
            perror("unlink fifo");
        }
    }
}

/* ========== Главная функция ========== */

/**
 * Главная функция программы
 * Обрабатывает аргументы командной строки и запускает соответствующую демонстрацию
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "pipe") == 0) {
        run_pipe_demo();
    } else if (strcmp(argv[1], "fifo-writer") == 0) {
        run_fifo_writer();
    } else if (strcmp(argv[1], "fifo-reader") == 0) {
        run_fifo_reader();
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
