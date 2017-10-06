#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <pthread.h>
#include "pin.H"

// Major settings regarding sync and round size
#define MAX_THREADS 64              // Max number of spawned threads by application
#define INSTRUCTIONS_ON_ROUND 1     // Number of instructions to be executed per round
#define INSTRUCTIONS_ON_EXIT  10000 // On exit, just give instructions to finish nicely

// --- Thread info ---

// Status structs Indicate current thread status regarding lock and step
typedef enum {
    LOCKED = 0,       // Waiting within a lock
    UNLOCKED = 1,     // Running other stuff free
    UNREGISTERED = 2, // Not registered yet, must use message
    FINISHED = 3,     // Already finished its job
    CREATING = 4,     // Using pthread_create
}   THREAD_STATUS;

typedef enum {
    STEP_MISS = 0,
    STEP_DONE = 1,
}   STEP_STATUS;

// Holds information of a given thread
typedef struct _THREAD_INFO THREAD_INFO;
struct _THREAD_INFO {
    INT64 ins_max;              // Current max of instructions to be executed
    INT64 ins_count;            // Number of instructions executed on current

    void *holder;               // Saves parameters from being dirty between before_* and after_* calls
    PIN_MUTEX wait_controller;  // Mutex used to wait controller answer

    pthread_t create_value;     // Thread variable returned by create, used for join control


    THREAD_STATUS status;       // Current Status of executing and step
    STEP_STATUS step_status;

    _THREAD_INFO *next;         // Linked list, used if on a waiting queue
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
    MSG_DONE = 0,
    MSG_REGISTER = 1,
    MSG_FINI = 2,
    MSG_BEFORE_LOCK = 3,
    MSG_BEFORE_TRY_LOCK = 4,
    MSG_BEFORE_UNLOCK = 5,
    MSG_BEFORE_CREATE = 6,
    MSG_BEFORE_JOIN = 7,
    MSG_AFTER_LOCK = 8,
    MSG_AFTER_TRY_LOCK = 9,
    MSG_AFTER_UNLOCK = 10,
    MSG_AFTER_CREATE = 11,
} MSG_TYPE;

typedef struct _MSG MSG;
struct _MSG {
    THREADID tid;
    MSG_TYPE msg_type;
    void *arg;
};

// MSG buffer, shared by all
extern MSG msg_buffer;

// Function used to when instrumentation should stop
void fail();

// Init controller and its communication
void controller_init();

// Send request to controller, concurrent allowed
void send_request(MSG msg);

// Controller main function, pintool should spawn a thread using it
void controller_main(void *arg);

// Allow a thread to run another step
void release_thread(THREAD_INFO *ti, INT64 instructions);

// If syncronized, release all unlocked threads for another step
void try_release_all();

// Debug funtion, print thread table on stderr
void print_threads();

#endif // CONTROLLER_H_
