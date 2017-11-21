#ifndef SYNC_H_
#define SYNC_H_

#include "pin.H"

// --- Communication related ---

// Used to lock controller and msg
extern PIN_MUTEX msg_mutex;
extern PIN_MUTEX controller_mutex;

typedef enum {
    ACTION_DONE = 0,
    ACTION_REGISTER = 1,
    ACTION_FINI = 2,
    ACTION_LOCK_DESTROY = 3,
    ACTION_LOCK_INIT = 4,
    ACTION_LOCK = 5,
    ACTION_TRY_LOCK = 6,
    ACTION_UNLOCK = 7,
    ACTION_BEFORE_CREATE = 8,
    ACTION_BEFORE_JOIN = 9,
    ACTION_AFTER_CREATE = 10,
    ACTION_SEM_DESTROY = 11,
    ACTION_SEM_GETVALUE = 12,
    ACTION_SEM_INIT = 13,
    ACTION_SEM_POST = 14,
    ACTION_SEM_TRYWAIT = 15,
    ACTION_SEM_WAIT = 16,
    ACTION_COND_BROADCAST = 17,
    ACTION_COND_DESTROY = 18,
    ACTION_COND_INIT = 19,
    ACTION_COND_SIGNAL = 20,
    ACTION_COND_WAIT = 21,
} ACTION_TYPE;

// Arguments are used to pass data to/from sync.
struct ACTION_ARG {
    void *p_1;
    void *p_2;
    int i;
};

typedef struct _ACTION ACTION;
struct _ACTION {
    THREADID tid;
    ACTION_TYPE action_type;
    ACTION_ARG arg;
};

// Init sync structure 
void sync_init();

// Perform a sync based on a valid action
void sync(ACTION *action);

#endif // SYNC_H_
