#ifndef LOCKHASH_H_
#define LOCKHASH_H_

/*
lockhash implement a simple hash for mutex pointers, using uthash
and pintool types. It meant to be used only by controller, since hash
table is accessible only by its thread. Since there is only one hash,
keeping a global value seems cheaper.
*/

#include "controller.h"
#include "uthash.h"
#include <pthread.h>

// Current lock status
typedef enum {
    M_LOCKED = 0,     // Currently locked: accepts try to pass, hold locks attempts.
    M_LOCKING = 1,    // Someone (only one) is locking either by lock or try_lock
    M_UNLOCKED = 2,   // Nothing happening, could be locked by a lucky thread.
    M_UNLOCKING = 3,  // Someone is currently unlocking, but haven't finished yet.
    M_WAITING = 4,    // Someone is trying to unlock, but there are thread(s) on try_lock.
                      // Should wait for them to finish, but deny new lock/try call.
}   LOCK_STATUS;

// Lock hash
typedef struct _MUTEX_ENTRY MUTEX_ENTRY;
struct _MUTEX_ENTRY{
    void * key;
    int threads_trying;             // Current trying threads
    LOCK_STATUS status;             // Current status of mutex

    UT_hash_handle hh;
    
    THREAD_INFO * locked;           // Waiting to go
    THREAD_INFO * about_try;        // About try lock (and fail)
    THREAD_INFO * about_unlock;     // About unlock, waiting try_locks
};

// Join Hash 
typedef struct _JOIN_ENTRY JOIN_ENTRY;
struct _JOIN_ENTRY{
    pthread_t key;

    UT_hash_handle hh;
    
    int allow;                      // Allow continue if thread exited already
    THREAD_INFO * locked;           // Waiting for given tread
};

// Handle functions to deal with each mutex function, during before and after.
// It will automatically update allThreads variable from controller.h.
void handle_before_lock(void * key, THREADID tid);
void handle_after_lock(void * key, THREADID tid);

void handle_before_try(void * key, THREADID tid);
void handle_after_try(void * key, THREADID tid);

void handle_before_unlock(void * key, THREADID tid);
void handle_after_unlock(void * key, THREADID tid);

// Used for join management
void handle_thread_exit(pthread_t key);
void handle_before_join(pthread_t key, THREADID tid);

// Used for locking reentrant lock.
typedef struct _REENTRANT_LOCK REENTRANT_LOCK;
struct _REENTRANT_LOCK {
    int busy;
    THREAD_INFO * locked;           // Threads locked
};

void handle_reentrant_start (REENTRANT_LOCK * rl, THREADID tid);
void handle_reentrant_exit (REENTRANT_LOCK * rl);

// Debug function, print lock hash on stderr
void print_hash();

#endif
