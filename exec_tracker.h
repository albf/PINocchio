#ifndef EXEC_TRACKER_H_
#define EXEC_TRACKER_H_

#include "thread.h"

// Init the static heap structures.
void exec_tracker_init();

// Insert a newly added thread, should be waiting.
void exec_tracker_insert(THREAD_INFO *t);

// Remove a thread that just ended.
void exec_tracker_remove(THREAD_INFO *);

// Mark a thread as sleeping, move from one heap to another.
void exec_tracker_sleep(THREAD_INFO *t);

// Mark a thread as running, move frome one heap to another.
void exec_tracker_awake();

// Debug function, print what heaps currently hold.
void exec_tracker_print();

// Return the first thread on the waiting heap.
THREAD_INFO * exec_tracker_peek_waiting();

// Return the first thread on the running heap.
THREAD_INFO * exec_tracker_peek_running();

#endif // EXEC_TRACKER_H_

