#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_LENGTH 64

char global_buffer[BUFFER_LENGTH];
sem_t sync_lock;

void* writer_func() {
    int value = 0;
    while (1) {
        sleep(1);
        sem_wait(&sync_lock);  // захват семафора
        value++;
        snprintf(global_buffer, BUFFER_LENGTH, "Value = %d", value);
        sem_post(&sync_lock);  // освобождение семафора
    }
    return NULL;
}

void* reader_func() {
    pthread_t thread_id = pthread_self();
    while (1) {
        sem_wait(&sync_lock);  // захват семафора
        printf("Reader ID: %lu | Buffer content: %s\n",
               (unsigned long)thread_id, global_buffer);
        sem_post(&sync_lock);  // освобождение семафора
        sleep(1);
    }
    return NULL;
}

int main(void) {
    pthread_t writer_thread_id, reader_thread_id;
    sem_init(&sync_lock, 0, 1);
    strcpy(global_buffer, "Empty");
    pthread_create(&writer_thread_id, NULL, writer_func, NULL);
    pthread_create(&reader_thread_id, NULL, reader_func, NULL);
    pthread_join(writer_thread_id, NULL);
    pthread_join(reader_thread_id, NULL);

    // Уничтожение семафора
    sem_destroy(&sync_lock);
    return 0;
}
