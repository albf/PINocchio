#include "log.h"

static THREADID watcher_tid;

void log_init(THREADID watcher) {
    watcher_tid = watcher;
}

void fail()
{
    PIN_ExitProcess(-1);
}

THREADID print_id(THREADID tid) {
    if (tid > watcher_tid) {
        return tid - 1;
    }
    return tid;
}
