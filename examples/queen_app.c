// Based on: https://github.com/tcreech/nqueens-pthreads
// Modified to fit our needs.

/**** N-queens since 2004-09   by Kenji KISE ****/
/**** N-queens POSIX Threads Version in C    ****/
/****  -pthreads adaption by tcreech@umd.edu ****/
/************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include "stopwatch.h"

/************************************************/
#define NUM_QUEEN 12
#define MAX 29 /** 32 is a real max! **/
#define MIN 2
#define MAX_TASK 100000

/************************************************/
typedef struct array_t {
    unsigned int cdt; /* candidates        */
    unsigned int col; /* column            */
    unsigned int pos; /* positive diagonal */
    unsigned int neg; /* negative diagonal */
} array;

int jobs, n;
int prob[MAX_TASK]; /* store problem IDs */
int rets[MAX_TASK]; /* store each answer */
unsigned grabbed = 0;
pthread_mutex_t grabbed_lock;

int atomic_grab_next(void)
{
    int retval = -1;
#ifdef USE_PTHREAD_SYNC
    pthread_mutex_lock(&grabbed_lock);
    if(grabbed < jobs) {
        retval = (++grabbed);
    }
    pthread_mutex_unlock(&grabbed_lock);
#else // USE_PTHREAD_SYNC
    unsigned local_grabbed = __sync_add_and_fetch(&grabbed, 1);
    if(local_grabbed <= jobs) {
        retval = local_grabbed;
    }
#endif // USE_PTHREAD_SYNC
    return retval;
}

/** N-queens kernel                            **/
/** n: problem size, h: height, r: row         **/
/************************************************/
long long qn(int n, int h, int r, array *a)
{
    long long answers = 0;

    for(;;) {
        if(r) {
            int lsb = (-r) & r;
            a[h + 1].cdt = (r & ~lsb);
            a[h + 1].col = (a[h].col & ~lsb);
            a[h + 1].pos = (a[h].pos |  lsb) << 1;
            a[h + 1].neg = (a[h].neg |  lsb) >> 1;

            r = a[h + 1].col & ~(a[h + 1].pos | a[h + 1].neg);
            h++;
        } else {
            r = a[h].cdt;
            h--;

            if(h == 0) {
                break;
            }
            if(h == n) {
                answers++;
            }
        }
    }
    return answers;
}

/** print the answer                           **/
/************************************************/
void print_result(int n, long long solution)
{
    int i;
    static long long answer[30] = {
        0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724,
        2680, 14200, 73712, 365596, 2279184,
        14772512, 95815104, 666090624ull,
        4968057848ull, 39029188884ull,
        314666222712ull, 2691008701644ull,
        24233937684440ull,
        0, 0, 0, 0, 0, 0
    };

    for(i = 0; i < 45; i++) {
        printf("=");
    }
    printf("\n");
    printf("total   solutions     : %lld\n",
           solution);
    if(solution != answer[n]) {
        printf(" ### Wrong answer\n");
    }
    for(i = 0; i < 45; i++) {
        printf("=");
    }
    printf("\n");
    fflush(stdout);
}

/** n-queens solver, specify first 4 level     **/
/************************************************/
long long queen_sub(int n, int *in, int mode)
{
    int i;
    long long solution = 0; /* solutions          */
    unsigned int row;       /* row candidate      */
    unsigned int h;         /* height or level    */
    array a[MAX];

    for(i = 0; i < MAX; i++) {
        a[i].cdt = a[i].col = a[i].pos = a[i].neg = 0;
    }
    row      = (1 << n) - 1;
    a[1].col = (1 << n) - 1;
    a[1].pos = 0;
    a[1].neg = 0;
    a[1].cdt = 1; /* care */
    a[2].cdt = 0; /* care */

    for(h = 1; h <= 4; h++) { /** Initialize **/
        int lsb = (1 << in[h]);

        if((a[h].col & lsb) == 0) {
            return 0;
        }
        if((a[h].pos & lsb) != 0) {
            return 0;
        }
        if((a[h].neg & lsb) != 0) {
            return 0;
        }

        a[h + 1].col = (a[h].col & ~lsb);
        a[h + 1].pos = (a[h].pos |  lsb) << 1;
        a[h + 1].neg = (a[h].neg |  lsb) >> 1;
        row = a[h + 1].col & ~(a[h + 1].pos | a[h + 1].neg);
    }

    if(mode == 0) {
        return 1;
    }

    solution = qn(n, h, row, a);
    return solution;
}

/** get the number of sub problems             **/
/************************************************/
int get_sub_problem_num(int n, int *prob)
{
    int i;
    int problem = 0;
    int max = ((n >> 1) + (n & 1)) * n * n * n;

    for(i = 0; i <= max; i++) {
        int in[MAX];
        in[1] = (i - 1) / n / n / n;
        in[2] = (i - 1) / n / n % n;
        in[3] = (i - 1) / n % n;
        in[4] = (i - 1) % n;

        if(queen_sub(n, in, 0)) {
            problem++;
            prob[problem] = i;
        }
    }
    printf("There are %d tasks\n", problem);
    if(problem > MAX_TASK) {
        printf("The number of tasks is too large.\n");
        exit(1);
    }
    return problem;
}

/************************************************/
long long solver(int n, int id,
                 int problem, int *prob)
{
    long long ret;
    int p_no = prob[id];
    int in[MAX];

    if(id < 1 || id > problem) {
        printf("The id %d is out of range.\n", id);
        return 0;
    }

    in[1] = (p_no - 1) / n / n / n;
    in[2] = (p_no - 1) / n / n % n;
    in[3] = (p_no - 1) / n % n;
    in[4] = (p_no - 1) % n;

    ret = queen_sub(n, in, 1);
    if(in[1] < (n >> 1)) {
        ret = ret * 2;
    }

    return ret;
}

void *worker_body(void *thread_v)
{
    int i;
    while((i = atomic_grab_next()) != -1) {
        rets[i] = solver(n, i, jobs, prob);
    }
    return NULL;
}

/** main function                              **/
/************************************************/
int main(int argc, char *argv[])
{
    stopwatch_start();
    int i;
    n    = NUM_QUEEN;
    jobs = get_sub_problem_num(n, prob);
    long long answers = 0;
    unsigned int nthreads = 1;

    if(argc > 1) {
        nthreads = atoi(argv[1]);
    }

    if(pthread_mutex_init(&grabbed_lock, NULL)) {
        fprintf(stderr, "error initializing mutex");
        return 3;
    }

    // Distribute work in a dynamic fashion, as in OMP_SCHEDULE=dynamic.
    pthread_t *workers = malloc(sizeof(pthread_t) * nthreads);
    for(int thread = 0; thread < nthreads; thread++) {
        pthread_create(workers + thread, NULL, &worker_body, NULL);
    }

    for(int thread = 0; thread < nthreads; thread++) {
        pthread_join(workers[thread], NULL);
    }
    free(workers);
    printf("All work finished.\n");

    for(i = 1; i <= jobs; i++) {
        answers += rets[i];
    }
    print_result(n, answers);
    stopwatch_stop();
    return 0;
}
/************************************************/