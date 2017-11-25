#ifndef LOCK_HASH_H_
#define LOCK_HASH_H_

/*
lock_hash implement a simple hashes for mutex, semaphores and joins.
It's meant to be used only by sync, since its functions changes thread status.
(It will modify status, but won't relase the threads)
*/

#include "thread.h"
#include "uthash.h"
#include <pthread.h>

/* Mutex Handlers */

// Just destroy a mutex. Will fail if doesn't exist.
void handle_mutex_destroy(void *key);

// Initialize the mutex. Will fail if rewriting a mutex with waiting threads.
void handle_lock_init(void *key);

// Will mark thread as locked and insert itself the waiting list or mark the mutex as locked.
void handle_lock(void *key, THREADID tid);

// Returns 0 if lock was successfull, 1 otherwise. Will fail if doesn't exist.
int handle_try_lock(void *key);

// Oposite of handle_lock, could awake someone or mark the mutex as unlocked.
void handle_unlock(void *key, THREADID tid);



/* Semaphore Handlers */

// Just destroy the semaphore. Will fail if doesn't exist.
void handle_semaphore_destroy(void *key);

// Just get the current value. Will fail if doesn't exist.
int handle_semaphore_getvalue(void *key);

// Initialize semaphore. Will fail if rewriting a semaphore with waiting threads.
void handle_semaphore_init(void *key, int value);

// Increase value by 1. Will fail if semaphore doesn't exist.
void handle_semaphore_post(void *key, THREADID tid);

// If value > 0, return 0 and decrease the value by 1. -1 otherwise. Will fail if doesn't exist.
int handle_semaphore_trywait(void *key);

//  Same as trywait, but will lock if no success. Will fail if doesn't exist.
void handle_semaphore_wait(void *key, THREADID tid);



/* rwlock Handlers */

// Just destroy the rwlock. Will fail if doesn't exist.
void handle_rwlock_destroy(void *key);

// Initialize rwlock. Will fail if rewriting a rwlock with waiting threads.
void handle_rwlock_init(void *key);

// Wait on the rwlock for reading. Will fail if doesn't exist.
void handle_rwlock_rdlock(void *key, THREADID tid);

// Wait on the rwlock for writing. Will fail if doesn't exist.
void handle_rwlock_wrlock(void *key, THREADID tid);

// Try to get rwlock for reading. Will fail if doesn't exist.
int handle_rwlock_tryrdlock(void *key, THREADID tid);

// Try to get rwlock for writing. Will fail if doesn't exist.
int handle_rwlock_trywrlock(void *key, THREADID tid);

// Unlock the rwlock. It takes into account the states of calling threads. Will fail if doesn't exist.
void handle_rwlock_unlock(void *key, THREADID tid);



/* Condition Variables Handlers */

// Wake all threads waiting on the condition variables.
void handle_cond_broadcast(void *key, THREADID tid);

// Just destroy the condition variable. Will fail if doesn't exist.
void handle_cond_destroy(void *key);

// Init the condition variable and should be called before anything.
void handle_cond_init(void *key);

// Wake up to one thread waiting (if any). Will fail if it doesn't exist.
void handle_cond_signal(void *key, THREADID tid);

// Lock on the condition variable. Will unlock the mutex. Will fail if doesn't exist.
void handle_cond_wait(void *key, void *mutex, THREADID tid);



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

// handle_reentrant_start should be called at the start of a exclusive function.
void handle_reentrant_start(REENTRANT_LOCK *rl, THREADID tid);

// handle_reentrant_exit oposite of start. Might unlock someone.
void handle_reentrant_exit(REENTRANT_LOCK *rl, THREADID tid);



// Debug function, print lock hash on stderr
void lock_hash_print_lock_hash();

// Debug function, print condition variable hash on stderr
void lock_hash_print_cond_hash();

#endif
