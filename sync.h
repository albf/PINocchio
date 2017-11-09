#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <pthread.h>
#include "pin.H"

// Major settings regarding sync and round size
#define MAX_THREADS 64               // Max number of spawned threads by application
#define INSTRUCTIONS_ON_ROUND 1      // Number of instructions to be executed per round
#define INSTRUCTIONS_ON_EXIT  100000 // On exit, just give instructions to finish nicely

// --- Thread info ---

// Status structs Indicate current thread status regarding lock and step
// Order will be considered preference on log merge
#define POSSIBLE_STATES 4
typedef enum {
    UNLOCKED = 0,     // Running other stuff free
    LOCKED = 1,       // Waiting within a lock
    UNREGISTERED = 2, // Not registered yet, must use message
    FINISHED = 3,     // Already finished its job
}   THREAD_STATUS;

typedef enum {
    STEP_MISS = 0,
    STEP_DONE = 1,
}   STEP_STATUS;

// Holds information of a given thread
typedef struct _THREAD_INFO THREAD_INFO;
struct _THREAD_INFO {
    INT64 ins_max;                  // Current max of instructions to be executed
    INT64 ins_count;                // Number of instructions executed on current

    void *holder;                   // Saves parameters from being dirty between before_* and after_* calls
    PIN_SEMAPHORE active;           // Semaphore used to wake/wait

    pthread_t create_value;         // Thread variable returned by create, used for join control


    THREAD_STATUS status;           // Current Status of executing and step
    STEP_STATUS step_status;

    _THREAD_INFO *next;             // Linked list, used if on a waiting queue
};

// THREAD_INFO declared on controller.h should be visible
// to all files
extern THREAD_INFO *all_threads;
extern THREADID max_tid;

// --- Communication related ---

// Used to lock controller and msg
extern PIN_MUTEX msg_mutex;
extern PIN_MUTEX controller_mutex;

typedef enum {
    ACTION_DONE = 0,
    ACTION_REGISTER = 1,
    ACTION_FINI = 2,
    ACTION_LOCK = 3,
    ACTION_TRY_LOCK = 4,
    ACTION_UNLOCK = 5,
    ACTION_BEFORE_CREATE = 6,
    ACTION_BEFORE_JOIN = 7,
    ACTION_AFTER_CREATE = 8,
} ACTION_TYPE;

// Arguments are used to pass data to/from sync. Since always one valued
// is passed, it can be an union.
union ACTION_ARG {
    int i;
    void *p;
};

typedef struct _ACTION ACTION;
struct _ACTION {
    THREADID tid;
    ACTION_TYPE action_type;
    ACTION_ARG arg;
};

// Function used to when instrumentation should stop
void fail();

// Init sync structure 
void sync_init();

// Perform a sync based on a valid action
void sync(ACTION *action);

// Debug funtion, print thread table on stderr
void print_threads();

#endif // CONTROLLER_H_
