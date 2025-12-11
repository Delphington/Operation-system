//#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// Объявления функций из utils.c
extern void format_time(time_t t, char *buf, size_t bufsz);
extern void die(const char *msg);

/**
 * Демонстрирует работу с pipe (именованным каналом между процессами)
 */
void run_pipe_demo(void) {
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

        printf("### PIPE (child) ###\n");
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

