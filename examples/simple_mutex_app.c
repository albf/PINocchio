#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define COUNT_MAX 10000

pthread_mutex_t mutex;
int y;

/* Dummy thread function */
void *inc_x(void *x_void_ptr)
{
    int *x_pt = (int *)x_void_ptr;

    /* increment y to COUNT_MAX */
    while(y < COUNT_MAX) {
        pthread_mutex_lock(&mutex);
        if(y < COUNT_MAX) {
            y++;
        }
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

int main(int argc , char **argv)
{
    int *x, i;
    int num_threads = 2;

    if(pthread_mutex_init(&mutex, NULL)) {
        fprintf(stderr, "error initializing mutex");
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

    pthread_mutex_destroy(&mutex);

    if(y != COUNT_MAX) {
        fprintf(stderr, "Internal Error: Result (%d) different than expected (%d)", y, COUNT_MAX);
        return 3;
    }

    printf("All threads joined.\n");

    free(x);
    free(inc_x_thread);
    return 0;
}
