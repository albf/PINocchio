#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"

int main(int argc , char **argv)
{
    stopwatch_start();
    printf("Hello World\n");
    stopwatch_stop();
    return 0;
}
