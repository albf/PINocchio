#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#define LOOP_COUNT 4000000 
#define SEM_VALUE 2

sem_t sem;
sem_t mutex;

int y;

int mat(int a, int b) {
    int sign = (b % 2 == 0) ? 1 : -1;
    return a * sign;
}

void *dummy_func(void *pn)
{
    int r = 0;
    int n = *((int *) pn);

    int OUTER_LOOP = n/1000;
    int INNER_LOOP = 1000;

    sem_wait(&sem);
    for(int i = 0; i < OUTER_LOOP; i++) {
        for(int j = 0; j < INNER_LOOP; j++) {
            r = r + mat(i,j);
        }
    }
    sem_post(&sem);

    // Should sum 0 here.
    sem_wait(&mutex);
    y = y + r;
    sem_post(&mutex);

    return NULL;
}

int main(int argc , char **argv)
{
    int i, *n;
    int num_threads = 2;

    if(sem_init(&sem, 0, SEM_VALUE)) {
        fprintf(stderr, "error initializing semaphore");
        return 3;
    }

    if(sem_init(&mutex, 0, 1)) {
        fprintf(stderr, "error initializing semaphore/mutex");
        return 3;
    }

    y = 0;

    if(argc > 1) {
        num_threads = atoi(argv[1]);
    }

    n = (int *) malloc(num_threads*sizeof(int));

    pthread_t *dummy_thread;
    dummy_thread = (pthread_t *) malloc(num_threads * sizeof(pthread_t));

    for(i = 0; i < num_threads; i++) {
            n[i] = LOOP_COUNT/(num_threads);
            if(pthread_create(&dummy_thread[i], NULL, dummy_func, &n[i])) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    for(i = 0; i < num_threads; i++) {
        fprintf(stdout, "JOINING THREAD\n");
        if(pthread_join(dummy_thread[i], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            return 2;
        }
    }

    if(sem_destroy(&sem)) {
        fprintf(stderr, "error destroying semaphore");
        return 4;
    }

    if(sem_destroy(&mutex)) {
        fprintf(stderr, "error destroying semaphore/mutex");
        return 4;
    }

    if(y != 0) {
        fprintf(stderr, "Internal Error: Result (%d) different than expected (%d)", y, 0);
        return 5;
    }

    printf("All threads joined.\n");

    free(n);
    free(dummy_thread);
    return 0;
}
