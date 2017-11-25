#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"

#define COUNT_MAX 10000

int *result;

/* Dummy thread function */
void *inc_x(void *x_void_ptr)
{
    int *x_pt = (int *)x_void_ptr;

    /* increment y to COUNT_MAX */
    int y = 0;
    while(++(y) < COUNT_MAX);

    result[*x_pt] = y;
    return NULL;
}

int main(int argc , char **argv)
{
    stopwatch_start();
    int *x, i;
    int num_threads = 2;

    if(argc > 1) {
        num_threads = atoi(argv[1]);
    }

    x = (int *) malloc(num_threads * sizeof(int));
    result = (int *) malloc(num_threads * sizeof(int));
    for(i = 0; i < num_threads; i++) {
        x[i] = i;
        result[i] = 0;
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
        if(pthread_join(inc_x_thread[i], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            return 2;
        }
        if(result[i] != COUNT_MAX) {
            fprintf(stderr, "Internal error: result[%d] = %d != %d", i, result[i], COUNT_MAX);
            return 3;
        }
    }

    printf("All threads joined.\n");

    free(x);
    free(result);
    free(inc_x_thread);
    stopwatch_stop();
    return 0;
}
