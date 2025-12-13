#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>

#define MAX_ATTEMPTS 5
#define SLEEP_INTERVAL 5

const char *SHARED_MEMORY_NAME = "/shm_time";
const char *SEMAPHORE_NAME     = "/sem_time";

// Структура данных для передачи
typedef struct {
    time_t time_value;
    pid_t  process_id;
    char   text[128];
} shared_block_t;

void cleanup(int signal_code) {
    shm_unlink(SHARED_MEMORY_NAME);
    sem_unlink(SEMAPHORE_NAME);
    exit(EXIT_SUCCESS);
}

void show_time_stamp(void) {
    char formatted_time[32];
    time_t current_time = time(NULL);

    strftime(formatted_time, sizeof(formatted_time),
             "%Y-%m-%d %H:%M:%S", localtime(&current_time));

    printf("Time: %s | PID: %d\n", formatted_time, getpid());
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    sem_t *semaphore = sem_open(SEMAPHORE_NAME, O_CREAT, 0644, 1);
    if (semaphore == SEM_FAILED) {
        perror("ERROR создания семафора");
        return EXIT_FAILURE;
    }

    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        int shared_fd = shm_open(SHARED_MEMORY_NAME, O_RDONLY, 0644);
        if (shared_fd != -1) {
            break;
        } else if (attempt == MAX_ATTEMPTS - 1) {
            perror("ERROR: открытия разделяемой памяти");
            sem_close(semaphore);
            return EXIT_FAILURE;
        } else {
            sleep(SLEEP_INTERVAL);
        }
    }

    int shared_fd = shm_open(SHARED_MEMORY_NAME, O_RDONLY, 0644);
    if (shared_fd == -1) {
        perror("ERROR: открытия разделяемой памяти");
        sem_close(semaphore);
        return EXIT_FAILURE;
    }

    shared_block_t *shared_data =
        mmap(NULL, sizeof(shared_block_t),
             PROT_READ, MAP_SHARED, shared_fd, 0);

    if (shared_data == MAP_FAILED) {
        perror("ERROR: отображения разделяемой памяти");
        close(shared_fd);
        sem_close(semaphore);
        return EXIT_FAILURE;
    }

    printf("Consumer started, pid=%d\n", getpid());

    while (1) {
        sem_wait(semaphore);

        if (strlen(shared_data->text) > 0) {
            printf("Consumer: %s\n", shared_data->text);
            show_time_stamp();
        } else {
            printf("No message consumer.\n");
        }

        sem_post(semaphore);
        sleep(SLEEP_INTERVAL);
    }

    munmap(shared_data, sizeof(shared_block_t));
    close(shared_fd);
    sem_close(semaphore);

    return 0;
}
