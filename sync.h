#ifndef SYNC_H_
#define SYNC_H_

#include "pin.H"

#define WATCHER_SLEEP 2     // Seconds between each watchers check.

// --- Communication related ---

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
    ACTION_RWLOCK_INIT = 17,
    ACTION_RWLOCK_DESTROY = 18,
    ACTION_RWLOCK_RDLOCK = 19,
    ACTION_RWLOCK_TRYRDLOCK = 20,
    ACTION_RWLOCK_WRLOCK = 21,
    ACTION_RWLOCK_TRYWRLOCK = 22,
    ACTION_RWLOCK_UNLOCK = 23,
    ACTION_COND_BROADCAST = 24,
    ACTION_COND_DESTROY = 25,
    ACTION_COND_INIT = 26,
    ACTION_COND_SIGNAL = 27,
    ACTION_COND_WAIT = 28,
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
void sync_init(int pram);

// Perform a sync based on a valid action
void sync(ACTION *action);

#endif // SYNC_H_
