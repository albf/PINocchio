/**
  Based on: http://nirbhay.in/blog/2013/07/producer_consumer_pthreads/

  @info A sample program to demonstrate the classic consumer/producer problem
  using pthreads. Modified to accept multiple consumers and do some dummy
  processing after each "consume".
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX 100                 /* production max value */
#define LOOP_MULTIPLIER 10000   /* loop of each produced task: LOOP_MULTIPLIER * TASK_ID */

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sig_consumer = PTHREAD_COND_INITIALIZER;
pthread_cond_t sig_producer = PTHREAD_COND_INITIALIZER;
int buffer, waiting;

int result[MAX];

void calculate_result(int task_id)
{
    int i, t_result = 0;
    for(i = 0; i < LOOP_MULTIPLIER * task_id; i++) {
        t_result++;
    }
    result[task_id] = t_result;
}

void *consumer(void *thread_data)
{
    int task;

    while(1) {
        pthread_mutex_lock(&mutex);
        waiting++;
        while(buffer == -1) {
            pthread_cond_signal(&sig_producer);
            pthread_cond_wait(&sig_consumer, &mutex);
        }
        task = buffer;
        buffer = -1;
        waiting--;
        if(waiting > 0) {
            pthread_cond_signal(&sig_producer);
        }
        pthread_mutex_unlock(&mutex);

        if(task == -2) {
            return;
        }

        calculate_result(task);
    }
}

void *producer(void *thread_data)
{
    int *num_threads = (int *) thread_data;
    int i, number = 0;

    for(i = 0; i < MAX; i++) {
        pthread_mutex_lock(&mutex);
        if((waiting == 0) || (buffer != -1)) {
            pthread_cond_wait(&sig_producer, &mutex);
        }
        buffer = i;
        pthread_cond_signal(&sig_consumer);
        pthread_mutex_unlock(&mutex);
    }

    for(i = 1; i < *num_threads; i++) {
        pthread_mutex_lock(&mutex);
        if((waiting == 0) || (buffer != -1)) {
            pthread_cond_wait(&sig_producer, &mutex);
        }
        buffer = -2;
        pthread_cond_signal(&sig_consumer);
        pthread_mutex_unlock(&mutex);
    }
}

int main(int argc , char **argv)
{
    int i, num_threads = 2;

    if(argc > 1) {
        num_threads = atoi(argv[1]);
        if(num_threads < 1) {
            fprintf(stderr, "num_threads must be at least 1");
        }
    }

    num_threads++;
    pthread_t *threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t));;
    pthread_cond_init(&sig_consumer, NULL);
    pthread_cond_init(&sig_producer, NULL);
    pthread_mutex_init(&mutex, NULL);
    buffer = -1;
    waiting = 0;

    if(pthread_create(&threads[0], NULL, producer, &num_threads)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    for(i = 1; i < num_threads; i++) {
        if(pthread_create(&threads[i], NULL, consumer, NULL)) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    for(i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_cond_destroy(&sig_consumer);
    pthread_cond_destroy(&sig_producer);
    pthread_mutex_destroy(&mutex);

    printf("All threads exit\n");
    return 0;
}
