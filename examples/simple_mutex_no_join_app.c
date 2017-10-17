#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define COUNT_MAX 10

pthread_mutex_t mutex;

typedef struct _ti ti;
struct _ti {
    pthread_mutex_t exit;
    int i;
};
int y;

/* Dummy thread function */
void *inc_x(void *x_void_ptr)
{
    ti *x_pt = (ti *)x_void_ptr;

    /* increment y to COUNT_MAX */
    while(y < COUNT_MAX) {
        pthread_mutex_lock(&mutex);
        if(y < COUNT_MAX) {
            y++;
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_unlock(&(x_pt->exit));
    return NULL;
}

int main(int argc , char **argv)
{
    int i, num_threads = 2;
    ti *x;

    if(pthread_mutex_init(&mutex, NULL)) {
        fprintf(stderr, "error initializing mutex");
        return 3;
    }
    y = 0;

    if(argc > 1) {
        num_threads = atoi(argv[1]);
    }

    x = (ti *) malloc(num_threads * sizeof(ti));
    for(i = 0; i < num_threads; i++) {
        x[i].i = i;
        pthread_mutex_init(&x[i].exit, NULL);
        // Custom made join initialize locked
        pthread_mutex_lock(&x[i].exit);
    }

    pthread_t *inc_x_thread;
    inc_x_thread = (pthread_t *) malloc(num_threads * sizeof(pthread_t));

    for(i = 0; i < num_threads; i++) {
        if(pthread_create(&inc_x_thread[i], NULL, inc_x, &x[i])) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    // Custom-made join
    for(i = 0; i < num_threads; i++) {
        pthread_mutex_lock(&x[i].exit);
    }

    pthread_mutex_destroy(&mutex);

    printf("All threads exit\n");
    return 0;
}
