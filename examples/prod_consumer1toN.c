#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define COUNT_C 100
#define COUNT_P 500

pthread_mutex_t mutex;
#define RUN 100

typedef struct _ti ti;
int y = 0;

/* Dummy thread function */
void *consumer(void *x_void_ptr)
{
    int c = 0;
    int v = 0;
    int i;

    /* increment y to COUNT_MAX */
    while(1) {
        pthread_mutex_lock(&mutex);
        if(y == 1) {
            y = 0;
            c++;
        }

        for(i = 0; i < RUN / 4; i++) {
            v++;
        }

        pthread_mutex_unlock(&mutex);

        for(i = 0; i < RUN; i++) {
            v++;
        }

        if(c >= COUNT_C) {
            break;
        }
    }

    return NULL;
}

void *producer(void *x_void_ptr)
{
    int c = 0;
    ti *x_pt = (ti *)x_void_ptr;

    while(1) {
        pthread_mutex_lock(&mutex);
        if(y == 0) {
            y = 1;
            c++;
        }
        pthread_mutex_unlock(&mutex);
        if(c >= COUNT_P) {
            break;
        }
    }

    return NULL;
}

int main(int argc , char **argv)
{
    int i, conv = 5;

    if(pthread_mutex_init(&mutex, NULL)) {
        printf("error initializing mutex");
        return 3;
    }
    y = 0;

    if(argc > 1) {
        conv = atoi(argv[1]);
    }

    pthread_t *inc_x_thread;
    pthread_t p;
    inc_x_thread = (pthread_t *) malloc(conv * sizeof(pthread_t));

    if(pthread_create(&p, NULL, producer, NULL)) {
        printf("Error creating thread\n");
        return 1;
    }

    printf("before creating \n");
    for(i = 0; i < conv; i++) {
        if(pthread_create(&inc_x_thread[i], NULL, consumer, NULL)) {
            printf("Error creating thread\n");
            return 1;
        }
        printf("Created thread %d\n", i);
    }

    printf("Waiting...\n");
    // Custom-made join
    pthread_join(p, NULL);
    for(i = 0; i < conv; i++) {
        pthread_join(inc_x_thread[i], NULL);
    }
    printf("All threads exit\n");

    return 0;
}
