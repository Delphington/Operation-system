#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_CAPACITY 10
#define READER_COUNT 10

static pthread_mutex_t sync_lock;                    // Мьютекс для синхронизации доступа к буферу
static int data_buffer[BUFFER_CAPACITY];             // Разделяемый буфер данных
static volatile int elements_written = 0;            // Количество записанных элементов (volatile для видимости изменений)

// Функция потока-писателя
static void* producer(void* unused) {
    (void)unused;
    while (elements_written < BUFFER_CAPACITY) {
        pthread_mutex_lock(&sync_lock);
        usleep(1000);
        if (elements_written < BUFFER_CAPACITY) {
            data_buffer[elements_written] = elements_written;
            printf("Writer updated array at index: %d\n", data_buffer[elements_written]);
            elements_written++;
        }

        pthread_mutex_unlock(&sync_lock);
        usleep(100000);
    }
    return NULL;
}

// Функция потока-читателя
static void* consumer(void* param) {
    int reader_id = *((int*)param);
    for (;;) {
        pthread_mutex_lock(&sync_lock);
        usleep(10000);

        // Чтение текущего состояния буфера
        printf("Reader %d reads array: ", reader_id);
        for (int idx = 0; idx < elements_written; idx++) {
            printf("%d ", data_buffer[idx]);
        }
        printf(", tid: [%lx]\n", pthread_self());

        if (elements_written >= BUFFER_CAPACITY) {
            pthread_mutex_unlock(&sync_lock);
            break;
        }

        pthread_mutex_unlock(&sync_lock);
        usleep(100000);
    }
    return NULL;
}

// Инициализация мьютекса
static int initialize_synchronization(void) {
    int result = pthread_mutex_init(&sync_lock, NULL);
    if (result != 0) {
        fprintf(stderr, "Error initializing mutex: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// Создание потока-писателя
static int spawn_producer_thread(pthread_t* producer_handle) {
    int result = pthread_create(producer_handle, NULL, producer, NULL);
    if (result != 0) {
        fprintf(stderr, "Error creating writer thread: %s\n", strerror(errno));
        pthread_mutex_destroy(&sync_lock);
        return -1;
    }
    return 0;
}

// Создание потоков-читателей
static int spawn_consumer_threads(pthread_t* consumer_handles, int* consumer_ids, pthread_t producer_handle) {
    for (int idx = 0; idx < READER_COUNT; idx++) {
        consumer_ids[idx] = idx;
        int result = pthread_create(&consumer_handles[idx], NULL, consumer, &consumer_ids[idx]);
        if (result != 0) {
            fprintf(stderr, "Error creating reader thread %d: %s\n", idx, strerror(errno));
            pthread_cancel(producer_handle);
            pthread_mutex_destroy(&sync_lock);
            return -1;
        }
    }
    return 0;
}

// Ожидание завершения всех потоков
static void wait_for_threads(pthread_t* consumer_handles, pthread_t producer_handle) {
    for (int idx = 0; idx < READER_COUNT; idx++) {
        void* thread_result = NULL;
        int join_status = pthread_join(consumer_handles[idx], &thread_result);
        if (join_status != 0) {
            int error_code = errno;
            printf("ERROR IN JOIN (reader %d): %s(%d)\n", idx, strerror(error_code), error_code);
        }
    }

    // Ожидаем завершения писателя
    void* thread_result = NULL;
    int join_status = pthread_join(producer_handle, &thread_result);
    if (join_status != 0) {
        int error_code = errno;
        printf("ERROR IN JOIN (writer): %s(%d)\n", strerror(error_code), error_code);
    }
}

int main(void) {
    pthread_t consumer_threads[READER_COUNT];
    pthread_t producer_thread;
    int consumer_identifiers[READER_COUNT];
    if (initialize_synchronization() != 0) {
        return 1;
    }
    if (spawn_producer_thread(&producer_thread) != 0) {
        return 1;
    }
    if (spawn_consumer_threads(consumer_threads, consumer_identifiers, producer_thread) != 0) {
        return 1;
    }

    wait_for_threads(consumer_threads, producer_thread);

    // Освобождение ресурсов мьютекса
    pthread_mutex_destroy(&sync_lock);
    return 0;
}