/* log.cpp
 *
 * Copyright (C) 2017 Alexandre Luiz Brisighello Filho
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "log.h"

static THREADID watcher_tid;

void log_init(THREADID watcher)
{
    watcher_tid = watcher;
}

void fail()
{
    PIN_ExitProcess(-1);
}

THREADID print_id(THREADID tid)
{
    if(tid > watcher_tid) {
        return tid - 1;
    }
    return tid;
}
