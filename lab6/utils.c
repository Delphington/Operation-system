//#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* ========== Вспомогательные функции ========== */

/**
 * Форматирует время в строку формата "YYYY-MM-DD HH:MM:SS"
 */
void format_time(time_t t, char *buf, size_t bufsz) {
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);
}

/**
 * Выводит сообщение об ошибке и завершает программу
 * @param msg - сообщение об ошибке
 */
void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/**
 * Создает путь к FIFO файлу на основе UID пользователя
 */
const char *make_fifo_path(void) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/demo_fifo_%d", (int)getuid());
    return path;
}

