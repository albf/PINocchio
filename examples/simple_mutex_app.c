#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define COUNT_MAX 10

pthread_mutex_t mutex;
int y;

/* Dummy thread function */
void *inc_x(void *x_void_ptr)
{
    int *x_pt = (int *)x_void_ptr;

    /* increment y to COUNT_MAX */
    while(y < COUNT_MAX) {
        pthread_mutex_lock(&mutex);
        y++;
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

int main(int argc , char **argv)
{
    int *x, i;
    int conv = 2;

    if(pthread_mutex_init(&mutex, NULL)) {
        printf("error initializing mutex");
        return 3;
    }
    y = 0;

    if(argc > 1) {
        conv = atoi(argv[1]);
    }

    x = (int *) malloc(conv * sizeof(int));
    for(i = 0; i < conv; i++) {
        x[i] = i + 1;
    }

    pthread_t *inc_x_thread;
    inc_x_thread = (pthread_t *) malloc(conv * sizeof(pthread_t));

    for(i = 0; i < conv; i++) {
        if(pthread_create(&inc_x_thread[i], NULL, inc_x, &x[i])) {
            printf("Error creating thread\n");
            return 1;
        }
    }

    for(i = 0; i < conv; i++) {
        if(pthread_join(inc_x_thread[i], NULL)) {
            printf("Error joining thread\n");
            return 2;
        }
    }

    printf("All threads joined.\n");
    pthread_mutex_destroy(&mutex);
    return 0;
}
