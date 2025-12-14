#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <errno.h>

#define SHM_SIZE 256
#define SEM_KEY 1234
#define SHM_KEY 5678

typedef struct {
    char data[SHM_SIZE];
} shared_data;

void sem_wait(int sem_id) {
    struct sembuf op = {0, -1, 0};
    semop(sem_id, &op, 1);
}

void sem_signal(int sem_id) {
    struct sembuf op = {0, 1, 0};
    semop(sem_id, &op, 1);
}

int main() {
    int shm_id, sem_id;
    shared_data *shm_ptr;

    sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id < 0) {
        perror("semget");
        exit(1);
    }

    shm_id = shmget(SHM_KEY, sizeof(shared_data), 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    shm_ptr = (shared_data *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Consumer started. PID: %d\n", getpid());
    printf("Waiting for messages...\n\n");

    while (1) {
        sem_wait(sem_id);

        time_t rawtime;
        struct tm *timeinfo;
        char buffer[80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

        // Выводим информацию
        printf("Consumer Time: %s\n", buffer);
        printf("Consumer PID: %d\n", getpid());
        printf("Consumer message: %s\n", shm_ptr->data);
        printf("----------------------------------------\n");
        sem_signal(sem_id);
        sleep(2);
    }
    shmdt(shm_ptr);
    return 0;
}