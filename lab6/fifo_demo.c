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

// Объявления функций из utils.c
extern void format_time(time_t t, char *buf, size_t bufsz);
extern void die(const char *msg);
extern const char *make_fifo_path(void);

/**
 * Процесс-писатель для FIFO
 */
void run_fifo_writer(void) {
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

    printf("fifo-writer: wrote\n");
}

/**
 * Процесс-читатель для демонстрации FIFO
 */
void run_fifo_reader(void) {
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

    printf("### FIFO(reader) ###\n");
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

