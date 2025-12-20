#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define ARRAY_SIZE 50
#define READERS_COUNT 10
#define WRITER_ITERATIONS 20

typedef struct {
    char buffer[ARRAY_SIZE];
    pthread_rwlock_t rwlock;
    int write_counter;
    int stop_flag;
} shared_data_t;

typedef struct {
    int id;
    shared_data_t *shared_data;
} reader_arg_t;

void* reader_thread(void* arg) {
    reader_arg_t* args = (reader_arg_t*)arg;
    shared_data_t* shared = args->shared_data;

    while (!shared->stop_flag) {
        pthread_rwlock_rdlock(&shared->rwlock);
        printf("Reader tid: %lu, Array: \"%s\"\n", pthread_self(), shared->buffer);
        pthread_rwlock_unlock(&shared->rwlock);
        usleep(50000 + (rand() % 100000));
    }

    free(args);
    return NULL;
}

void* writer_thread(void* arg) {
    shared_data_t* shared = (shared_data_t*)arg;
    strcpy(shared->buffer, "Initial");

    for (int i = 0; i < WRITER_ITERATIONS; i++) {
        pthread_rwlock_wrlock(&shared->rwlock);
        shared->write_counter++;
        snprintf(shared->buffer, ARRAY_SIZE, "Write #%d", shared->write_counter);
        printf("Writer: Записал в массив: %s\n", shared->buffer);
        pthread_rwlock_unlock(&shared->rwlock);
        usleep(200000 + (rand() % 300000));
    }

    shared->stop_flag = 1;
    usleep(300000);
    printf("Writer завершил работу после %d записей\n", WRITER_ITERATIONS);
    return NULL;
}

int main() {
    pthread_t readers[READERS_COUNT];
    pthread_t writer;
    shared_data_t shared_data;

    srand(time(NULL));

    memset(&shared_data, 0, sizeof(shared_data_t));
    shared_data.write_counter = 0;
    shared_data.stop_flag = 0;
    strcpy(shared_data.buffer, "Empty");

    if (pthread_rwlock_init(&shared_data.rwlock, NULL) != 0) {
        perror("Ошибка инициализации rwlock");
        return 1;
    }

    printf("============================================================\n");
    printf("Лабораторная работа №10\n");
    printf("Создано %d читающих потоков и 1 пишущий\n", READERS_COUNT);
    printf("Размер общего массива: %d символов\n", ARRAY_SIZE);
    printf("Пишущий поток выполнит %d записей\n", WRITER_ITERATIONS);
    printf("============================================================\n\n");

    for (int i = 0; i < READERS_COUNT; i++) {
        reader_arg_t* arg = malloc(sizeof(reader_arg_t));
        if (!arg) {
            perror("Ошибка выделения памяти");
            return 1;
        }
        arg->id = i + 1;
        arg->shared_data = &shared_data;

        if (pthread_create(&readers[i], NULL, reader_thread, arg) != 0) {
            perror("Ошибка создания потока-читателя");
            free(arg);
            return 1;
        }
    }

    if (pthread_create(&writer, NULL, writer_thread, &shared_data) != 0) {
        perror("Ошибка создания потока-писателя");
        return 1;
    }

    pthread_join(writer, NULL);

    for (int i = 0; i < READERS_COUNT; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_rwlock_destroy(&shared_data.rwlock);

    printf("\n============================================================\n");
    printf("Все потоки завершены успешно\n");
    printf("Финальное состояние массива: \"%s\"\n", shared_data.buffer);
    printf("============================================================\n");

    return 0;
}
