#ifndef LOCKHASH_H_
#define LOCKHASH_H_

/*
lockhash implement a simple hashes for mutex, semaphores and joins.
It's meant to be used only by sync, since its functions changes thread status.
(It will modify status, but won't relase the threads)
*/

#include "sync.h"
#include "uthash.h"
#include <pthread.h>

/* Mutex Handlers */

// Returns 0 if lock was successfull, 1 otherwise.
int handle_lock(void *key, THREADID tid);

// Returns 0 if lock was successfull, 1 otherwise.
int handle_try_lock(void *key);

// Returns the awake thread, if any.
THREAD_INFO * handle_unlock(void *key);



/* Semaphore Handlers */

// Just destroy the semaphore. Will fail if doesn't exist.
void handle_semaphore_destroy(void *key);

// Just get the current value. Will fail if doesn't exist.
int handle_semaphore_getvalue(void *key);

// Initialize semaphore. Will fail if rewriting a semaphore with waiting threads.
void handle_semaphore_init(void *key, int value);

// Increase value by 1. Will fail if semaphore doesn't exist.
THREAD_INFO *handle_semaphore_post(void *key);

// If value > 0, return 1 and decrease the value by 1. -1 otherwise. Will fail if doesn't exist.
int handle_semaphore_trywait(void *key);

//  Same as trywait, but will lock if no success. Will fail if doesn't exist.
void handle_semaphore_wait(void *key, THREADID tid);



/* Thread create/exit Handlers */ 

// Returns a list with threads waiting to join.
THREAD_INFO * handle_thread_exit(pthread_t key);

// Returns 1 if allowed, 0 if not allowed.
int handle_before_join(pthread_t key, THREADID tid);



/* Reentrant Lock (function lock) */

// Used for locking reentrant lock.
typedef struct _REENTRANT_LOCK REENTRANT_LOCK;
struct _REENTRANT_LOCK {
    int busy;
    THREAD_INFO *locked;            // Threads locked
};

// handle_reentrant_start returns 0 if it get locked, 1 otherwise.
int handle_reentrant_start(REENTRANT_LOCK *rl, THREADID tid);

// handle_reentrant_exit will return a thread to get into the reentrant function or null.
THREAD_INFO * handle_reentrant_exit(REENTRANT_LOCK *rl);



// Debug function, print lock hash on stderr
void print_hash();

#endif
