/* main_app.c
 *
 * Copyright (C) 2017 Alexandre Luiz Brisighello Filho
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"

int main(int argc, char **argv)
{
    stopwatch_start();
    printf("Hello World\n");
    stopwatch_stop();
    return 0;
}
