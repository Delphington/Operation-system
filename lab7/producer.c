#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define SLEEP_INTERVAL 5

const char *SHARED_MEM_ID = "/shm_time";
const char *SEM_ID = "/shm_sem";

// Структура данных для передачи
typedef struct {
    time_t time_value;
    pid_t process_id;
    char text[128];
} ipc_payload_t;

sem_t *ipc_sem;

void release_resources(void) {
    shm_unlink(SHARED_MEM_ID);
    sem_close(ipc_sem);
    sem_unlink(SEM_ID);
}

void on_signal(int sig) {
    release_resources();
    exit(0);
}

int main() {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    ipc_sem = sem_open(SEM_ID, O_CREAT | O_EXCL, 0644, 1);
    if (ipc_sem == SEM_FAILED) {
        if (errno == EEXIST) {
            fprintf(stderr, "Процесс уже запущен! pid =%d\n", getpid());
            return EXIT_FAILURE;
        } else {
            perror("ERROR: создания семафора");
            return EXIT_FAILURE;
        }
    }

    int mem_fd = shm_open(SHARED_MEM_ID, O_CREAT | O_RDWR, 0644);
    if (mem_fd == -1) {
        perror("ERROR: создания разделяемой памяти");
        sem_close(ipc_sem);
        sem_unlink(SEM_ID);
        return EXIT_FAILURE;
    }

    if (ftruncate(mem_fd, sizeof(ipc_payload_t)) == -1) {
        perror("ERROR: установки размера разделяемой памяти");
        shm_unlink(SHARED_MEM_ID);
        sem_close(ipc_sem);
        sem_unlink(SEM_ID);
        return EXIT_FAILURE;
    }

    ipc_payload_t *shared_block = mmap(
        NULL,
        sizeof(ipc_payload_t),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        0
    );

    if (shared_block == MAP_FAILED) {
        perror("ERROR: отображения разделяемой памяти");
        shm_unlink(SHARED_MEM_ID);
        sem_close(ipc_sem);
        sem_unlink(SEM_ID);
        return EXIT_FAILURE;
    }

    close(mem_fd);

    printf("Producer started, pid=%d\n", getpid());

    while (1) {
        shared_block->time_value = time(NULL);
        shared_block->process_id = getpid();

        char time_str[80];
        strftime(
            time_str,
            sizeof(time_str),
            "%Y-%m-%d %H:%M:%S",
            localtime(&shared_block->time_value)
        );

        snprintf(
            shared_block->text,
            sizeof(shared_block->text),
            "Time: %s, PID: %d",
            time_str,
            shared_block->process_id
        );

        sleep(SLEEP_INTERVAL);
    }

    munmap(shared_block, sizeof(ipc_payload_t));
    release_resources();

    return 0;
}
