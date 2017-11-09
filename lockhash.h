#ifndef LOCKHASH_H_
#define LOCKHASH_H_

/*
lockhash implement a simple hash for mutex pointers, using uthash
and pintool types. It meant to be used only by sync, since its
functions changes thread status. Since there is only one hash for
each type, keeping a global value seems cheaper.
*/

#include "sync.h"
#include "uthash.h"
#include <pthread.h>

// Current lock status
typedef enum {
    M_LOCKED = 0,     // Currently locked: accepts try to pass, hold locks attempts.
    M_UNLOCKED = 1,   // Nothing happening, could be locked by a lucky thread.
}   LOCK_STATUS;

// Lock hash
typedef struct _MUTEX_ENTRY MUTEX_ENTRY;
struct _MUTEX_ENTRY {
    void *key;
    LOCK_STATUS status;             // Current status of mutex

    UT_hash_handle hh;

    THREAD_INFO *locked;            // Waiting to go
};

// Join Hash
typedef struct _JOIN_ENTRY JOIN_ENTRY;
struct _JOIN_ENTRY {
    pthread_t key;

    UT_hash_handle hh;

    int allow;                      // Allow continue if thread exited already
    THREAD_INFO *locked;            // Waiting for given tread
};

// Handle functions to deal with each mutex function, during execution.
// It will automatically update allThreads variable from controller.h.
// Return values helps sync define when to check other threads status.
int handle_lock(void *key, THREADID tid);
int handle_try_lock(void *key, THREADID tid);

// handle_Unlock returns the awake thread, if any.
THREAD_INFO * handle_unlock(void *key, THREADID tid);

// Used for join management

// handle_thread_exit returns a list with threads waiting to join.
THREAD_INFO * handle_thread_exit(pthread_t key);

// handle_before_join returns 1 if allowed, 0 if not allowed.
int handle_before_join(pthread_t key, THREADID tid);

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
