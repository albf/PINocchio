#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define COUNT_MAX 10
#define NEXEC 10000

pthread_mutex_t mutex;

typedef struct {
    long long unsigned int n;   //Number of executions
    long long unsigned int in;  //Space to save the result
    unsigned int seedp;         //Store the state for rand_r()
    pthread_mutex_t exit;
} T_thread_param;

/* Dummy thread function */
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
    pthread_mutex_unlock(&(lv->exit));
    return NULL;
}


int main(int argc , char **argv)
{
    int i, conv = 2;

    if(argc > 1) {
        conv = atoi(argv[1]);
    }

    pthread_t *inc_x_thread;
    inc_x_thread = (pthread_t *) malloc(conv * sizeof(pthread_t));
    T_thread_param *tp = malloc(sizeof(T_thread_param) * conv);

    for(i = 0; i < conv; i++) {
        pthread_mutex_init(&tp[i].exit, NULL);
        pthread_mutex_lock(&tp[i].exit);
        tp[i].seedp = i;
    }
    int points_to_distr = NEXEC;
    for(i = 0; i < conv; i++) {
        tp[i].n = points_to_distr / (conv - i);
        tp[i].seedp = i;  // Avoid threads to compute same calculation
        points_to_distr -= tp[i].n;
    }

    srand(time(NULL));


    printf("before creating \n");
    for(i = 0; i < conv; i++) {
        if(pthread_create(&inc_x_thread[i], NULL, parallel_pi, (void *)&tp[i])) {
            printf("Error creating thread\n");
            return 1;
        }
        printf("Created thread %d\n", i);
    }

    printf("Waiting...\n");
    // Custom-made join
    for(i = 0; i < conv; i++) {
        pthread_mutex_lock(&tp[i].exit);
    }
    printf("All threads exit\n");

    int in = 0;
    for(i = 0; i < conv; i++) {
        in += tp[i].in;
    }

    double pi = 4 * in / ((double)NEXEC);
    printf("%lf\n", pi);


    return 0;
}
