#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define COUNT_MAX 10

/* Dummy thread function */
void *inc_x(void *x_void_ptr) {
    int *x_pt = (int *)x_void_ptr;
    printf("Thread #%d: Created\n", *x_pt);

    /* increment y to COUNT_MAX */
    int y;
    while(++(y) < COUNT_MAX);

    printf("Thread #%d: y increment finished\n", *x_pt);
    return NULL;
}

int main(int argc , char **argv) {
    int * x, i; 
    int conv = 2;

    if(argc > 1) {
        conv = atoi(argv[1]);
    }

    x = (int *) malloc(conv*sizeof(int));
    for(i = 0; i < conv; i++) {
        x[i] = i+1;
    }

    pthread_t * inc_x_thread;
    inc_x_thread = (pthread_t *) malloc (conv*sizeof(pthread_t));

    for(i = 0; i < conv; i++) {
        if(pthread_create(&inc_x_thread[i], NULL, inc_x, &x[i])) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    for(i = 0; i < conv; i++) {
        if(pthread_join(inc_x_thread[i], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            return 2;
        }
    }

    printf("All threads joined.\n");
    return 0;
}
