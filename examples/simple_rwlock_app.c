/* simple_rwlock_app.c
 *
 * Copyright (C) 2017 Alexandre Luiz Brisighello Filho
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"

#define COUNT_MAX 20000

pthread_rwlock_t rwlock;
int y;

int mat(int a, int b)
{
    int sign = (b % 2 == 0) ? 1 : -1;
    return a * sign;
}

/* Dummy thread function */
void *inc_x(void *x_void_ptr)
{
    int *x_pt = (int *)x_void_ptr;
    int local = 0;

    // Threads with higher TID will finish slower.
    pthread_rwlock_rdlock(&rwlock);
    for(int i = 0; i < (COUNT_MAX * (*x_pt)); i++) {
        local += mat(i, i + 1);
    }
    pthread_rwlock_unlock(&rwlock);

    // Write the non-sense on the global variable
    pthread_rwlock_wrlock(&rwlock);
    y += local;
    pthread_rwlock_unlock(&rwlock);

    return NULL;
}

int main(int argc, char **argv)
{
    stopwatch_start();
    int *x, i;
    int num_threads = 2;

    if(pthread_rwlock_init(&rwlock, NULL)) {
        fprintf(stderr, "error initializing rwlock");
        return 3;
    }
    y = 0;

    if(argc > 1) {
        num_threads = atoi(argv[1]);
    }

    x = (int *) malloc(num_threads * sizeof(int));
    for(i = 0; i < num_threads; i++) {
        x[i] = i + 1;
    }

    pthread_t *inc_x_thread;
    inc_x_thread = (pthread_t *) malloc(num_threads * sizeof(pthread_t));

    for(i = 0; i < num_threads; i++) {
        if(pthread_create(&inc_x_thread[i], NULL, inc_x, &x[i])) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    for(i = 0; i < num_threads; i++) {
        fprintf(stdout, "JOINING THREAD\n");
        if(pthread_join(inc_x_thread[i], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            return 2;
        }
    }

    pthread_rwlock_destroy(&rwlock);


    printf("y: %d\n", y);
    printf("All threads joined.\n");

    free(x);
    free(inc_x_thread);
    stopwatch_stop();
    return 0;
}
