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

    // Создаем семафор
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("semget");
        exit(1);
    }

    // Инициализируем семафор значением 1
    semctl(sem_id, 0, SETVAL, 1);

    // Создаем разделяемую память
    shm_id = shmget(SHM_KEY, sizeof(shared_data), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    // Присоединяем разделяемую память
    shm_ptr = (shared_data *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Producer started. PID: %d\n", getpid());

    while (1) {
        sleep(3);

        time_t rawtime;
        struct tm *timeinfo;
        char buffer[80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

        char message[SHM_SIZE];
        snprintf(message, sizeof(message), "Time: %s, Sender PID: %d", buffer, getpid());

        sem_wait(sem_id);

        strncpy(shm_ptr->data, message, SHM_SIZE - 1);
        shm_ptr->data[SHM_SIZE - 1] = '\0';
        printf("Producer: Sent message: %s\n", message);
        sem_signal(sem_id);
    }

    // Отсоединяем разделяемую память
    shmdt(shm_ptr);

    return 0;
}