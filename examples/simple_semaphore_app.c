/* simple_semaphore_app.c
 *
 * Copyright (C) 2017 Alexandre Luiz Brisighello Filho
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"

#define LOOP_COUNT 200000
#define SEM_VALUE 2

sem_t sem;
sem_t mutex;

int y;

struct _dummy_work {
    int outer_loop;
    int inner_loop_start;
    int inner_loop_end;
};

typedef struct _dummy_work DUMMY_WORK;

int mat(int a, int b)
{
    int sign = (b % 2 == 0) ? 1 : -1;
    return a * sign;
}

void *dummy_func(void *pn)
{
    int r = 0;
    DUMMY_WORK * dw = (DUMMY_WORK *) pn;

    sem_wait(&sem);
    for(int i = 0; i < dw->outer_loop; i++) {
        for(int j = dw->inner_loop_start; j < dw->inner_loop_end; j++) {
            r = r + mat(i, j);
        }
    }
    sem_post(&sem);

    // Should sum 0 here.
    sem_wait(&mutex);
    y = y + r;
    sem_post(&mutex);

    return NULL;
}

int main(int argc, char **argv)
{
    stopwatch_start();
    int i;
    int num_threads = 2;
    DUMMY_WORK * n;

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

    n = (DUMMY_WORK *) malloc(num_threads * sizeof(DUMMY_WORK));

    pthread_t *dummy_thread;
    dummy_thread = (pthread_t *) malloc(num_threads * sizeof(pthread_t));

    int reminder = LOOP_COUNT % num_threads;
    int block_size = (int) (LOOP_COUNT/num_threads);

    for(i = 0; i < num_threads; i++) {
        //n[i] = LOOP_COUNT / (num_threads);
        n[i].outer_loop = 1000;
        n[i].inner_loop_start = (block_size*i);
        n[i].inner_loop_end = n[i].inner_loop_start + block_size + reminder;

        reminder = 0;

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
    stopwatch_stop();
    return 0;
}
