#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <pthread.h>
#include "pin.H"

// Major settings regarding sync and round size
#define MAX_THREADS 1024             // Max number of spawned threads by application
#define INSTRUCTIONS_ON_ROUND 1      // Number of instructions to be executed per round
#define INSTRUCTIONS_ON_DELAY 0      // Instructions to be executed on a delay (to be revisited)
#define MAX_DELAYS 1000              // Max number of delays on a given thread

// --- Thread info ---

// Indicate current thread status regarding lock
typedef enum {
    LOCKED = 0,    // Waiting within a lock
    UNLOCKED = 1,  // Running other stuff free
}   THREAD_STATUS;

// Holds information of all thread
typedef struct _THREAD_INFO THREAD_INFO;
struct _THREAD_INFO {
    // Current max of instructions to be executed
    INT64 ins_max;
    // Number of instructions executed on current
    INT64 ins_count;
    // Saves mutexes from being dirty between before_mutex* and after_mutex* calls
    pthread_mutex_t* holder;
    // Mutex used to wait controller answer
    PIN_MUTEX wait_controller;

    // Current Status
    THREAD_STATUS status;

    // Linked list, used if on a waiting queue
    _THREAD_INFO * next;
};

// THREAD_INFO declared on controller.h should be visible
// to all files, including log and mutex hash.
extern THREAD_INFO * all_threads;
extern int max_tid;

// --- Communication related ---

// Used to lock controller and msg
extern PIN_MUTEX msg_mutex;
extern PIN_MUTEX controller_mutex;

// MSG types
typedef enum {
    MSG_DONE,
    MSG_REGISTER,
    MSG_BEFORE_LOCK,
    MSG_BEFORE_TRY_LOCK,
    MSG_BEFORE_UNLOCK,
    MSG_AFTER_LOCK,
    MSG_AFTER_TRY_LOCK,
    MSG_AFTER_UNLOCK
} MSG_TYPE;

// MSG struct
typedef struct _MSG MSG;
struct _MSG {
    INT64 tid;
    MSG_TYPE msg_type;
    void * arg;
};

// MSG buffer
extern MSG msg_buffer;

// --- HOLDER --- TODO: Remove holder use

// Temporary structure to force sync
typedef struct _HOLDER HOLDER;
struct _HOLDER {
    int max_tid;
    int * states;

    // flushes counter
    int sync_flushes;
    int delayed_flushes;
};
extern HOLDER thread_holder;

// Function used to when instrumentation should stop
void fail();

// Init controller and its communication
void controller_init();

// Send request to controller, concurrent allowed
void send_request(MSG msg);

// Controller main function, pintool should spawn a thread using it
void controller_main(void * arg);

#endif // CONTROLLER_H_
