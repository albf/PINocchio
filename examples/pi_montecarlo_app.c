#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"

#define NEXEC 100000

typedef struct {
    long long unsigned int n;   //Number of executions
    long long unsigned int in;  //Space to save the result
    unsigned int seedp;         //Store the state for rand_r()
} T_thread_param;

long long unsigned int monte_carlo_pi(unsigned int n, unsigned int *seedp)
{
    unsigned int in = 0, i;
    double x, y, d;
    for(i = 0; i < n; i++) {
        x = ((rand_r(seedp) % 1000000) / 500000.0) - 1;
        y = ((rand_r(seedp) % 1000000) / 500000.0) - 1;
        d = ((x * x) + (y * y));
        if(d <= 1) {
            in += 1;
        }
    }

    return in;
}

void *parallel_pi(void *v)
{
    T_thread_param *lv = ((T_thread_param *) v);

    lv->in = monte_carlo_pi(lv->n, &(lv->seedp));
    return NULL;
}

int main(int argc , char **argv)
{
    stopwatch_start();
    int i, num_threads = 2;

    if(argc > 1) {
        num_threads = atoi(argv[1]);
    }

    T_thread_param *tp = malloc(sizeof(T_thread_param) * num_threads);
    pthread_t *worker_threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t));

    for(i = 0; i < num_threads; i++) {
        tp[i].seedp = i;
    }
    int points_to_distr = NEXEC;
    for(i = 0; i < num_threads; i++) {
        tp[i].n = points_to_distr / (num_threads - i);
        tp[i].seedp = i;  // Avoid threads to compute same calculation
        points_to_distr -= tp[i].n;
    }

    srand(time(NULL));

    for(i = 0; i < num_threads; i++) {
        if(pthread_create(&worker_threads[i], NULL, parallel_pi, (void *)&tp[i])) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    for(i = 0; i < num_threads; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    int in = 0;
    for(i = 0; i < num_threads; i++) {
        in += tp[i].in;
    }

    double pi = 4 * in / ((double)NEXEC);
    printf("Pi result: %lf\n", pi);

    free(tp);
    free(worker_threads);
    stopwatch_stop();
    return 0;
}
