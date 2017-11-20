#ifndef THREAD_H_
#define THREAD_H_

#define MAX_THREADS 64               // Max number of spawned threads by application

#include <pthread.h>
#include "pin.H"

// --- Thread info ---

typedef enum {
    UNLOCKED = 0,     // Running other stuff free
    LOCKED = 1,       // Waiting within a lock
    UNREGISTERED = 2, // Not registered yet, must use message
    FINISHED = 3,     // Already finished its job
}   THREAD_STATUS;

// Holds information of a given thread
typedef struct _THREAD_INFO THREAD_INFO;
struct _THREAD_INFO {
    INT64 ins_count;                // Number of instructions executed on current

    void *holder;                   // Saves parameters from being dirty between before_* and after_* calls
                                    // *holder is also used to save mutex used on condition variables.

    PIN_SEMAPHORE active;           // Semaphore used to wake/wait

    THREADID pin_tid;               // It's own pin tid. It's needed when on a list
    pthread_t create_value;         // Thread variable returned by create, used for join control


    THREAD_STATUS status;           // Current Status of executing and step

    _THREAD_INFO *next_lock;        // Linked list, used if on a lock queue (lock_hash)
    _THREAD_INFO *next_running;     // Linked list, used if on the waiting queue (exec_tracker)
};

// THREAD_INFO declared on controller.h should be visible
// to all files
extern THREAD_INFO *all_threads;
extern THREADID max_tid;

// Init threads control structures 
void thread_init();

// Try, based on the heaps and internal states, to release threads.
void thread_try_release_all();

// Returns 1 if all threads have finished, 0 otherwise.
int thread_all_finished();

// Thread updates handlers. Will update both exec_tracker and trace_bank.
// Using these functions will allow thread_try_release_all to function correctly.

void thread_start(THREAD_INFO *target, THREAD_INFO *creator);

void thread_finish(THREAD_INFO *target);

void thread_lock(THREAD_INFO *target);

void thread_unlock(THREAD_INFO *target, THREAD_INFO *unlocker);

void thread_sleep(THREAD_INFO *target);

// Debug funtion, print thread table on stderr
void print_threads();

#endif // THREAD_H_
