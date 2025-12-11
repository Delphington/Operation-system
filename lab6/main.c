//#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Объявления функций из других модулей
extern void run_pipe_demo(void);
extern void run_fifo_writer(void);
extern void run_fifo_reader(void);

static void usage(const char *prog) {
    fprintf(stderr,
            "Use:\n"
            "1)  %s pipe\n"
            "2)  %s fifo-writer\n"
            "3)  %s fifo-reader\n\n"
            "Examples:\n"
            "    pipe demo (single run):\n"
            "1)  %s pipe\n\n"
            "2)  fifo demo (two separate processes):\n"
            "3)  In terminal 1:\n"
            "4)  %s fifo-reader\n"
            "5)  In terminal 2:\n"
            "6)  %s fifo-writer\n",
            prog, prog, prog, prog, prog, prog);
}

/* ========== Главная функция ========== */


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