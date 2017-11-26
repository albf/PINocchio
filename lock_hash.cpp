#include <iostream>
#include "lock_hash.h"
#include "log.h"
#include "pin.H"

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

// Semaphore hash
typedef struct _SEMAPHORE_ENTRY SEMAPHORE_ENTRY;
struct _SEMAPHORE_ENTRY {
    void *key;
    int value;                      // Current value of semaphore

    UT_hash_handle hh;

    THREAD_INFO *locked;            // Waiting to go
};

// Current read write lock status
typedef enum {
    RW_READING = 0,     // Currently locked reading, at least by one thread.
    RW_WRITING = 1,     // Currently locked writing: by one and only one thread.
    RW_UNLOCKED = 2,    // Currenty unlocked and could be unlocked freely.
}   RWLOCK_STATUS;

// Read Write hash
typedef struct _RWLOCK_ENTRY RWLOCK_ENTRY;
struct _RWLOCK_ENTRY {
    void *key;
    RWLOCK_STATUS status;

    UT_hash_handle hh;

    THREAD_INFO *users;             // Current users of the lock
    THREAD_INFO *locked;            // Waiting to go
};

// Condition Variable hash
typedef struct _COND_ENTRY COND_ENTRY;
struct _COND_ENTRY {
    void *key;

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

// Since there is only one hash for each type, keeping a global value seems cheaper.
static MUTEX_ENTRY *mutex_hash = NULL;
static SEMAPHORE_ENTRY *semaphore_hash = NULL;
static RWLOCK_ENTRY *rwlock_hash = NULL;
static COND_ENTRY *cond_hash = NULL;
static JOIN_ENTRY *join_hash = NULL;

// get_mutex_entry will find a given entry or, if doesn't exist, create one.
static MUTEX_ENTRY *get_mutex_entry(void *key)
{
    MUTEX_ENTRY *s;

    HASH_FIND_PTR(mutex_hash, &key, s);
    if(s) {
        return s;
    }

    return NULL;
}

static void delete_mutex_entry(MUTEX_ENTRY *entry)
{
    HASH_DEL(mutex_hash, entry);
}

static void initialize_mutex(MUTEX_ENTRY *s, void *key)
{
    s->key = key;
    s->status = M_UNLOCKED;
    s->locked = NULL;
}

static MUTEX_ENTRY *add_mutex_entry(void *key)
{
    MUTEX_ENTRY *s;

    s = (MUTEX_ENTRY *) malloc(sizeof(MUTEX_ENTRY));
    initialize_mutex(s, key);

    HASH_ADD_PTR(mutex_hash, key, s);
    return s;
}

// Insert a given THREAD_INFO on a linked list. Returns the new pointer
// to the list head.
static THREAD_INFO *insert(THREAD_INFO *list, THREAD_INFO *entry)
{
    entry->next_lock = NULL;

    // Unique entry on the moment
    if(list == NULL) {
        return entry;
    }

    // Add on the end of the list
    THREAD_INFO *t;
    for(t = list; t->next_lock != NULL; t = t->next_lock);
    t->next_lock = entry;
    return list;
}

static void insert_locked(MUTEX_ENTRY *mutex, THREAD_INFO *entry)
{
    mutex->locked = insert(mutex->locked, entry);
}

// Can't fail on no_mutex it's used by system during runtime Assume a new one.
static MUTEX_ENTRY *handle_no_mutex(MUTEX_ENTRY *s, void *key)
{
    if(s == NULL) {
        s = add_mutex_entry(key);
        cerr << "[PINocchio] Warning: Non-existent mutex acessed: " << key << ". Implicit entry created." << std::endl;
    }
    return s;
}

void handle_mutex_destroy(void *key)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);

    // Mutex doesn't even exist. Just return.
    if(s == NULL) {
        cerr << "[PINocchio] Warning: Destroy on already unexistent mutex." << std::endl;
        return;
    }

    // Destroying a mutex with other threads waiting.
    if(s->locked != NULL) {
        cerr << "Error: Mutex destroyed when other threads are waiting." << std::endl;
        fail();
    }

    delete_mutex_entry(s);
}

void handle_lock_init(void *key)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);

    if(s == NULL) {
        add_mutex_entry(key);
        return;
    }
    if(s->locked != NULL) {
        cerr << "Mutex destroyed (by init) when other threads are waiting." << std::endl;
        fail();
    }

    // Exists but no one is waiting. Just initialize it.
    initialize_mutex(s, key);
}

void handle_lock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);
    s = handle_no_mutex(s, key);

    if(s->status == M_UNLOCKED) {
        // If unlocked, first to come, just lock.
        s->status = M_LOCKED;
        return;
    }

    // If locked, just insert on list and mark as thread as locked.
    THREAD_INFO *t = &all_threads[tid];
    thread_lock(t);
    insert_locked(s, t);
    return;
}

int handle_try_lock(void *key)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);
    handle_no_mutex(s, key);

    if(s->status == M_UNLOCKED) {
        s->status = M_LOCKED;
        return 0;
    }
    return 1;
}

void handle_unlock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);
    handle_no_mutex(s, key);

    if(s->locked != NULL) {
        s->status = M_LOCKED;
        thread_unlock(s->locked, &all_threads[tid]);

        s->locked = s->locked->next_lock;
        return;
    }

    if(s->status == M_LOCKED) {
        s->status = M_UNLOCKED;
    } else {
        // Odd case, won't change anything.
        cerr << "[PINocchio] Warning: Unlock on already unlocked mutex." << std::endl;
    }

    return;
}

static void insert_semaphore_locked(SEMAPHORE_ENTRY *sem, THREAD_INFO *entry)
{
    sem->locked = insert(sem->locked, entry);
}

// get_semaphore_entry will find a given entry or return null.
static SEMAPHORE_ENTRY *get_semaphore_entry(void *key)
{
    SEMAPHORE_ENTRY *s;

    HASH_FIND_PTR(semaphore_hash, &key, s);
    if(s) {
        return s;
    }

    return NULL;
}

static void delete_semaphore_entry(SEMAPHORE_ENTRY *entry)
{
    HASH_DEL(semaphore_hash, entry);
}

static void initialize_semaphore(SEMAPHORE_ENTRY *s, void *key, int value)
{
    s->key = key;
    s->value = value;
    s->locked = NULL;
}

static void add_semaphore_entry(void *key, int value)
{
    SEMAPHORE_ENTRY *s;

    s = (SEMAPHORE_ENTRY *) malloc(sizeof(SEMAPHORE_ENTRY));
    initialize_semaphore(s, key, value);
    HASH_ADD_PTR(semaphore_hash, key, s);
}

static void fail_on_no_semaphore(SEMAPHORE_ENTRY *s, void *key)
{
    if(s == NULL) {
        cerr << "Error: Non-existent semaphore acessed: " << key << "." << std::endl;
        fail();
    }
}

void handle_semaphore_destroy(void *key)
{
    SEMAPHORE_ENTRY *s = get_semaphore_entry(key);

    // Semaphore doesn't even exist. Just return.
    if(s == NULL) {
        cerr << "[PINocchio] Warning: Destroy on already unexistent semaphore." << std::endl;
        return;
    }

    // Destroying a semaphore with other threads waiting.
    if(s->locked != NULL) {
        cerr << "Error: Semaphore destroyed when other threads are waiting." << std::endl;
        fail();
    }

    delete_semaphore_entry(s);
}

int handle_semaphore_getvalue(void *key)
{
    SEMAPHORE_ENTRY *s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    return s->value;
}

void handle_semaphore_init(void *key, int value)
{
    SEMAPHORE_ENTRY *s = get_semaphore_entry(key);

    if(s == NULL) {
        add_semaphore_entry(key, value);
        return;
    }
    if(s->locked != NULL) {
        cerr << "Error: Semaphore destroyed (by init) when other threads are waiting." << std::endl;
        fail();
    }

    // Exists but no one is waiting. Just initialize it.
    initialize_semaphore(s, key, value);
}

void handle_semaphore_post(void *key, THREADID tid)
{
    SEMAPHORE_ENTRY *s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    if(s->locked != NULL) {
        thread_unlock(s->locked, &all_threads[tid]);
        s->locked = s->locked->next_lock;
    }

    s->value = s->value + 1;
    return;
}

int handle_semaphore_trywait(void *key)
{
    SEMAPHORE_ENTRY *s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    if(s->value > 0) {
        s->value = s->value - 1;
        return 0;
    }
    return -1;
}

void handle_semaphore_wait(void *key, THREADID tid)
{
    SEMAPHORE_ENTRY *s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    if(s->value > 0) {
        s->value = s->value - 1;
        return;
    }

    THREAD_INFO *t = &all_threads[tid];
    thread_lock(&all_threads[tid]);

    insert_semaphore_locked(s, t);
    return;
}




static void insert_rwlock_locked(RWLOCK_ENTRY *rw, THREAD_INFO *entry)
{
    rw->locked = insert(rw->locked, entry);
}

static void insert_rwlock_users(RWLOCK_ENTRY *rw, THREAD_INFO *entry)
{
    rw->users = insert(rw->users, entry);
}

// get_rwlock_entry will find a given entry or return null.
static RWLOCK_ENTRY *get_rwlock_entry(void *key)
{
    RWLOCK_ENTRY *rw;

    HASH_FIND_PTR(rwlock_hash, &key, rw);
    if(rw) {
        return rw;
    }

    return NULL;
}

static void delete_rwlock_entry(RWLOCK_ENTRY *entry)
{
    HASH_DEL(rwlock_hash, entry);
}

static void initialize_rwlock(RWLOCK_ENTRY *rw, void *key)
{
    rw->key = key;
    rw->locked = NULL;
    rw->users = NULL;
    rw->status = RW_UNLOCKED;
}

static void add_rwlock_entry(void *key)
{
    RWLOCK_ENTRY *rw;

    rw = (RWLOCK_ENTRY *) malloc(sizeof(RWLOCK_ENTRY));
    initialize_rwlock(rw, key);
    HASH_ADD_PTR(rwlock_hash, key, rw);
}

static void fail_on_no_rwlock(RWLOCK_ENTRY *rw, void *key)
{
    if(rw == NULL) {
        cerr << "Error: Non-existent read write lock acessed: " << key << "." << std::endl;
        fail();
    }
}

void handle_rwlock_init(void *key)
{
    RWLOCK_ENTRY *rw = get_rwlock_entry(key);

    if(rw == NULL) {
        add_rwlock_entry(key);
        return;
    }
    if(rw->locked != NULL) {
        cerr << "Error: Read write lock destroyed (by init) when other threads are waiting." << std::endl;
        fail();
    }

    // Exists but no one is waiting. Just initialize it.
    initialize_rwlock(rw, key);
}

void handle_rwlock_destroy(void *key)
{
    RWLOCK_ENTRY *rw = get_rwlock_entry(key);

    // Read write lock doesn't even exist. Just return.
    if(rw == NULL) {
        cerr << "[PINocchio] Warning: Destroy on already unexistent read write lock." << std::endl;
        return;
    }

    // Destroying a read write lock :with other threads waiting.
    if(rw->locked != NULL) {
        cerr << "Error: Read write lock destroyed when other threads are waiting." << std::endl;
        fail();
    }

    delete_rwlock_entry(rw);
}

void handle_rwlock_rdlock(void *key, THREADID tid)
{
    RWLOCK_ENTRY *rw = get_rwlock_entry(key);
    all_threads[tid].holder = (pthread_t) RW_READING;
    fail_on_no_rwlock(rw, key);

    switch(rw->status) {
    case RW_UNLOCKED:
        rw->status = RW_READING;
    // Don't break, should add to users
    case RW_READING:
        insert_rwlock_users(rw, &all_threads[tid]);
        break;
    case RW_WRITING:
        // Can't take it. Make it as waiting for a read.
        insert_rwlock_locked(rw, &all_threads[tid]);
        thread_lock(&all_threads[tid]);
        break;
    }

    return;
}

int handle_rwlock_tryrdlock(void *key, THREADID tid)
{
    RWLOCK_ENTRY *rw = get_rwlock_entry(key);
    int response = 1;

    fail_on_no_rwlock(rw, key);

    switch(rw->status) {
    case RW_UNLOCKED:
        rw->status = RW_READING;
    // Don't break, should add to users
    case RW_READING:
        insert_rwlock_users(rw, &all_threads[tid]);
        all_threads[tid].holder = (pthread_t) RW_READING;
        response = 0;

        break;
    case RW_WRITING:
        // Can't take it but won't wait for it.
        break;
    }

    return response;
}

void handle_rwlock_wrlock(void *key, THREADID tid)
{
    RWLOCK_ENTRY *rw = get_rwlock_entry(key);
    all_threads[tid].holder = (void *) RW_WRITING;
    fail_on_no_rwlock(rw, key);

    switch(rw->status) {
    case RW_UNLOCKED:
        rw->status = RW_WRITING;
        insert_rwlock_users(rw, &all_threads[tid]);
        break;

    // Can't take it. Make it as waiting for a read.
    case RW_READING:
    case RW_WRITING:
        insert_rwlock_locked(rw, &all_threads[tid]);
        thread_lock(&all_threads[tid]);
        break;
    }

    return;
}

int handle_rwlock_trywrlock(void *key, THREADID tid)
{
    RWLOCK_ENTRY *rw = get_rwlock_entry(key);
    int response = 1;

    fail_on_no_rwlock(rw, key);

    switch(rw->status) {
    case RW_UNLOCKED:
        rw->status = RW_WRITING;
        all_threads[tid].holder = (void *) RW_WRITING;
        insert_rwlock_users(rw, &all_threads[tid]);
        response = 0;
        break;

    // Can't take it. Just return failure.
    case RW_READING:
    case RW_WRITING:
        break;
    }

    return response;
}

static void rwlock_remove_user(RWLOCK_ENTRY *rw, THREAD_INFO *t)
{
    if(rw->users == t) {
        rw->users = rw->users->next_lock;
        return;
    }
    THREAD_INFO *w;
    for(w = rw->users; w->next_lock != t; w = w->next_lock);
    w->next_lock = w->next_lock->next_lock;
}

static void fail_rwlock_wrong_type_unlock(void *key)
{
    cerr << "[PINocchio] Error: unlocking a rwlock from a wrong thread/type: " << key << "." << std::endl;
    fail();
}

void handle_rwlock_unlock(void *key, THREADID tid)
{
    RWLOCK_ENTRY *rw = get_rwlock_entry(key);
    THREAD_INFO *t = &all_threads[tid];
    RWLOCK_STATUS unlock_type;
    fail_on_no_rwlock(rw, key);

    switch(rw->status) {
    case RW_UNLOCKED:
        cerr << "[PINocchio] Warning: unlocking a rwlock already unlocked: " << key << "." << std::endl;
        break;

    // Can't take it. Make it as waiting for a read.
    case RW_READING:
        unlock_type = (RWLOCK_STATUS)((int64_t)t->holder);
        if(unlock_type == RW_WRITING) {
            fail_rwlock_wrong_type_unlock(key);
        }
        // Remove from user and check what situation we are facing
        rwlock_remove_user(rw, t);

        if(rw->users == NULL) {
            // No one else is reading anymore. Very nice, check if there is someone to wake.
            rw->status = RW_UNLOCKED;

            // It was in reading mode, if someone is waiting it is a writing one.
            if(rw->locked != NULL) {
                THREAD_INFO *awake = rw->locked;
                rw->locked = rw->locked->next_lock;
                insert_rwlock_users(rw, awake);
                thread_unlock(awake, t);
                rw->status = RW_WRITING;
            }
        }
        // Else: If it was locked for read and there still people reading, status remain the same.
        break;

    case RW_WRITING:
        unlock_type = (RWLOCK_STATUS)((int64_t)t->holder);
        if(unlock_type == RW_READING) {
            fail_rwlock_wrong_type_unlock(key);
        }
        // Remove from user and check what situation we are facing
        rwlock_remove_user(rw, t);

        // rw->users is always NULL, it's in writing mode and is getting removed.
        if(rw->locked != NULL) {
            // Find who should be awaken type.
            if((RWLOCK_STATUS)((int64_t)rw->locked->holder) == RW_WRITING) {
                THREAD_INFO *awake = rw->locked;
                rw->locked = rw->locked->next_lock;
                insert_rwlock_users(rw, awake);
                thread_unlock(awake, &all_threads[tid]);
            } else {
                // More complicated case, it's a reading request. Awake everyone.
                for(THREAD_INFO *awake = rw->locked; awake != NULL; awake = awake->next_lock) {
                    if(awake->next_lock != NULL && ((RWLOCK_STATUS)((int64_t)awake->next_lock->holder) == RW_READING)) {
                        insert_rwlock_users(rw, awake);
                        thread_unlock(awake->next_lock, t);
                        awake->next_lock = awake->next_lock->next_lock;
                    }
                }
                // Remove the first one.
                insert_rwlock_users(rw, rw->locked);
                thread_unlock(rw->locked, t);
                rw->locked = rw->locked->next_lock;

                // Finally mark status
                rw->status = RW_READING;
            }

        } else {
            // Guess no one is waiting. Be cool.
            rw->status = RW_UNLOCKED;
        }
        break;
    }
    return;
}







static COND_ENTRY *get_cond_entry(void *key)
{
    COND_ENTRY *c;

    HASH_FIND_PTR(cond_hash, &key, c);
    if(c) {
        return c;
    }

    return NULL;
}

static void delete_cond_entry(COND_ENTRY *entry)
{
    HASH_DEL(cond_hash, entry);
}

static void initialize_cond(COND_ENTRY *c, void *key)
{
    c->key = key;
    c->locked = NULL;
}

static COND_ENTRY *add_cond_entry(void *key)
{
    COND_ENTRY *c;

    c = (COND_ENTRY *) malloc(sizeof(COND_ENTRY));
    initialize_cond(c, key);

    HASH_ADD_PTR(cond_hash, key, c);
    return c;
}

static void insert_cond_locked(COND_ENTRY *c, THREAD_INFO *entry)
{
    c->locked = insert(c->locked, entry);
}

static void fail_on_no_cond(COND_ENTRY *s, void *key)
{
    if(s == NULL) {
        cerr << "[PINocchio] Error: Non-existent condition variable acessed: " << key << "." << std::endl;
        fail();
    }
}

static void cond_to_mutex(THREAD_INFO *t, THREADID tid)
{
    MUTEX_ENTRY *s = get_mutex_entry(t->holder);
    s = handle_no_mutex(s, t->holder);

    if(s->status == M_UNLOCKED) {
        // If unlocked, first to come, just lock.
        s->status = M_LOCKED;
        thread_unlock(t, &all_threads[tid]);
        return;
    }

    insert_locked(s, t);
}

void handle_cond_broadcast(void *key, THREADID tid)
{
    COND_ENTRY *c = get_cond_entry(key);
    fail_on_no_cond(c, key);

    // Unlock from condition variable but lock on the mutex.
    for(THREAD_INFO *t = c->locked; t != NULL; t = t->next_lock) {
        cond_to_mutex(t, tid);
    }
    c->locked = NULL;
}

void handle_cond_destroy(void *key)
{
    COND_ENTRY *c = get_cond_entry(key);

    // Condition variable doesn't even exist. Just return.
    if(c == NULL) {
        cerr << "[PINocchio] Warning: Destroy on already unexistent condition." << std::endl;
        return;
    }

    // Destroying a condition variable with other threads waiting.
    if(c->locked != NULL) {
        cerr << "Error: Semaphore destroyed when other threads are waiting." << std::endl;
        fail();
    }

    delete_cond_entry(c);
}

void handle_cond_init(void *key)
{
    COND_ENTRY *c = get_cond_entry(key);

    if(c == NULL) {
        add_cond_entry(key);
        return;
    }
    if(c->locked != NULL) {
        cerr << "Error: Condition variable destroyed (by init) when other threads are waiting." << std::endl;
        fail();
    }

    // Exists but no one is waiting. Just initialize it.
    initialize_cond(c, key);
}

void handle_cond_signal(void *key, THREADID tid)
{
    COND_ENTRY *c = get_cond_entry(key);
    fail_on_no_cond(c, key);

    // Unlock up to one, if exist. Unlock from conditional variable,
    // but lock on mutex. It could be awake or not, depending on the mutex.
    if(c->locked != NULL) {
        THREAD_INFO *t = c->locked;
        c->locked = c->locked->next_lock;
        cond_to_mutex(t, tid);
    }
}

void handle_cond_wait(void *key, void *mutex, THREADID tid)
{
    COND_ENTRY *c = get_cond_entry(key);
    fail_on_no_cond(c, key);

    // Save mutex for later use and lock thread.
    THREAD_INFO *t = &all_threads[tid];
    t->holder = mutex;
    thread_lock(&all_threads[tid]);
    handle_unlock(mutex, tid);

    // Insert as locked for the request condition variable.
    insert_cond_locked(c, t);
    return;
}

// Used to debug lock hash states
void lock_hash_print_lock_hash()
{
    MUTEX_ENTRY *s;
    const char *status[] = {"M_LOCKED", "M_UNLOCKED"};

    cerr << "--------- mutex table ---------" << std::endl;
    for(s = mutex_hash; s != NULL; s = (MUTEX_ENTRY *) s->hh.next) {
        cerr << "Key: " << s->key << " - status: " << status[s->status];
        if(s != NULL) {
            cerr << " - locked: ";
            for(THREAD_INFO *t = s->locked; t != NULL; t = t->next_lock) {
                cerr << t->pin_tid << " | ";
            }
        }
        cerr << std::endl;
    }
    cerr << "--------- ----------- ---------" << std::endl;
}

// Used to debug lock hash states
void lock_hash_print_cond_hash()
{
    COND_ENTRY *s;

    cerr << "--------- condition variable table ---------" << std::endl;
    for(s = cond_hash; s != NULL; s = (COND_ENTRY *) s->hh.next) {
        cerr << "  - Key: " << s->key << std::endl;
        if(s != NULL) {
            cerr << "  - locked: ";
            for(THREAD_INFO *t = s->locked; t != NULL; t = t->next_lock) {
                cerr << t->pin_tid << " | ";
            }
        }
        cerr << std::endl;
    }
    cerr << "--------- ----------- ---------" << std::endl;
}

// get_mutex_entry will find a given entry or, if doesn't exist, create one.
JOIN_ENTRY *get_join_entry(pthread_t key)
{
    JOIN_ENTRY *s;

    HASH_FIND(hh, join_hash, &key, sizeof(pthread_t), s);
    if(s) {
        return s;
    }

    // Not found, add new and return it.
    s = (JOIN_ENTRY *) malloc(sizeof(JOIN_ENTRY));
    s->key = key;
    s->allow = 0;
    s->locked = NULL;

    HASH_ADD(hh, join_hash, key, sizeof(pthread_t), s);
    return s;
}

// Handle a thread exit request in terms of join.
// Mark allow as 1, not stopping any other join.
// Return the list of locked threads on join.
THREAD_INFO *handle_thread_exit(pthread_t key)
{
    JOIN_ENTRY *s = get_join_entry(key);
    s->allow = 1;

    return s->locked;
}

// handle_before_join deals with join request. If
// allow, just release thread as usual - do nothing.
// If not, lock on thread exitence and check if others
// running threads could continue.
int handle_before_join(pthread_t key, THREADID tid)
{
    JOIN_ENTRY *s = get_join_entry(key);
    THREAD_INFO *t = &all_threads[tid];

    if(s->allow == 0) {
        thread_lock(t);
        s->locked = insert(s->locked, t);
        return 0;
    }

    return 1;
}

// handle_reentrant_start will deal with the case
// of a starting reentrat function that the tool
// wants to lock.
void handle_reentrant_start(REENTRANT_LOCK *rl, THREADID tid)
{
    THREAD_INFO *t = &all_threads[tid];

    if(rl->busy > 0) {
        thread_lock(t);
        rl->locked = insert(rl->locked, t);
        return;
    }

    rl->busy = 1;
    return;
}

// handle_reentrant_exit will check if there is
// any thread awaiting to get into the function and
// update it.
void handle_reentrant_exit(REENTRANT_LOCK *rl, THREADID tid)
{
    if(rl->locked == NULL) {
        rl->busy = 0;
        return;
    }

    thread_unlock(rl->locked, &all_threads[tid]);

    rl->locked = rl->locked->next_lock;
    return;
}
