/* stopwatch.c
 *
 * Copyright (C) 2017 Alexandre Luiz Brisighello Filho
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <time.h>
#include <sys/time.h>
#include <stdio.h>

static struct timespec ts_start, ts_end;
static struct timeval tv_start, tv_end;
static clock_t c_start;


void __attribute__((optimize("O0"))) stopwatch_start()
{
    gettimeofday(&tv_start, NULL);
    c_start = clock();
}

void __attribute__((optimize("O0"))) stopwatch_stop()
{
    gettimeofday(&tv_end, NULL);

    double wall = (1000000.0 * (double)(tv_end.tv_sec - tv_start.tv_sec));
    wall += ((double)(tv_end.tv_usec - tv_start.tv_usec));

    double cpu = 1000000 * (double)(clock() - c_start) / CLOCKS_PER_SEC;

    printf("@WALL: %lf\n", wall);
    printf("@CPU: %lf\n", cpu);
}
