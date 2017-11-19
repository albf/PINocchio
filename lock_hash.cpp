#include <iostream>
#include "lock_hash.h"
#include "error.h"
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

// Join Hash
typedef struct _JOIN_ENTRY JOIN_ENTRY;
struct _JOIN_ENTRY {
    pthread_t key;

    UT_hash_handle hh;

    int allow;                      // Allow continue if thread exited already
    THREAD_INFO *locked;            // Waiting for given tread
};

// Since there is only one hash for each type, keeping a global value seems cheaper.
MUTEX_ENTRY *mutex_hash = NULL;
SEMAPHORE_ENTRY *semaphore_hash = NULL;
JOIN_ENTRY *join_hash = NULL;

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

static void delete_mutex_entry(MUTEX_ENTRY * entry)
{
    HASH_DEL(mutex_hash, entry);
}

static void initialize_mutex(MUTEX_ENTRY *s, void *key)
{
    s->key = key;
    s->status = M_UNLOCKED;
    s->locked = NULL;
}

static MUTEX_ENTRY * add_mutex_entry(void * key) {
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
static MUTEX_ENTRY * handle_no_mutex(MUTEX_ENTRY *s, void * key)
{
    if(s == NULL) {
        s = add_mutex_entry(key);
        cerr << "Warning: Non-existent mutex acessed: " << key << ". Implicit entry created." << std::endl;
    }
    return s;
}

void handle_mutex_destroy(void *key)
{
    MUTEX_ENTRY * s = get_mutex_entry(key);

    // Mutex doesn't even exist. Just return.
    if(s == NULL) {
        cerr << "Warning: Destroy on already unexistent mutex." << std::endl;
        return;
    }

    // Destroying a mutex with other threads waiting.
    if(s->locked != NULL) {
        cerr << "Mutex destroyed when other threads are waiting." << std::endl;
        fail();
    }

    delete_mutex_entry(s);
}

void handle_lock_init(void *key)
{
    MUTEX_ENTRY * s = get_mutex_entry(key);

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

int handle_lock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);
    s = handle_no_mutex(s, key);

    if (s->status == M_UNLOCKED) {
        // If unlocked, first to come, just start locking.
        s->status = M_LOCKED;
        return 0;
    }

    // If locked, just insert on list and mark as thread as locked.
    THREAD_INFO *t = &all_threads[tid];
    thread_lock(t);
    insert_locked(s, t);
    return 1;
}

int handle_try_lock(void *key)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);
    handle_no_mutex(s, key);

    if (s->status == M_UNLOCKED) {
        s->status = M_LOCKED;
        return 0;
    }
    return 1;
}

// Handle_unlock returns the data from the awaked thread.
// If none was awaked, just return NULL.
THREAD_INFO * handle_unlock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_mutex_entry(key);
    handle_no_mutex(s, key);

    if(s->locked != NULL) {
        s->status = M_LOCKED;
        thread_unlock(s->locked, &all_threads[tid]);

        THREAD_INFO * awaked = s->locked;
        s->locked = s->locked->next_lock;
        return awaked;
    }

    if (s->status == M_LOCKED) {
        s->status = M_UNLOCKED;
    } else {
        // Odd case, won't change anything.
        cerr << "Warning: Unlock on already unlocked mutex." << std::endl;
    }

    return NULL; 
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

static void delete_semaphore_entry(SEMAPHORE_ENTRY * entry) {
    HASH_DEL(semaphore_hash, entry);
}

static void initialize_semaphore(SEMAPHORE_ENTRY *s, void *key, int value) {
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

static void fail_on_no_semaphore(SEMAPHORE_ENTRY *s, void * key) {
    if(s == NULL) {
        cerr << "Non-existent semaphore acessed: " << key << "." << std::endl;
        fail();
    }
}

void handle_semaphore_destroy(void *key)
{
    SEMAPHORE_ENTRY * s = get_semaphore_entry(key);

    // Semaphore doesn't even exist. Just return.
    if(s == NULL) {
        cerr << "Warning: Destroy on already unexistent semaphore." << std::endl;
        return;
    }

    // Destroying a semaphore with other threads waiting.
    if(s->locked != NULL) {
        cerr << "Semaphore destroyed when other threads are waiting." << std::endl;
        fail();
    }

    delete_semaphore_entry(s);
}

int handle_semaphore_getvalue(void *key)
{
    SEMAPHORE_ENTRY * s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    return s->value;
}

void handle_semaphore_init(void *key, int value)
{
    SEMAPHORE_ENTRY * s = get_semaphore_entry(key);

    if(s == NULL) {
        add_semaphore_entry(key, value);
        return;
    }
    if(s->locked != NULL) {
        cerr << "Semaphore destroyed (by init) when other threads are waiting." << std::endl;
        fail();
    }

    // Exists but no one is waiting. Just initialize it.
    initialize_semaphore(s, key, value);
}

THREAD_INFO *handle_semaphore_post(void *key, THREADID tid)
{
    SEMAPHORE_ENTRY * s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    if(s->locked != NULL) {
        thread_unlock(s->locked, &all_threads[tid]);

        THREAD_INFO * awaked = s->locked;
        s->locked = s->locked->next_lock;
        return awaked;
    }

    s->value = s->value + 1;
    return NULL;
}

int handle_semaphore_trywait(void *key)
{
    SEMAPHORE_ENTRY * s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    if(s->value > 0) {
        s->value = s->value -1;
        return 0;
    }
    return -1;
}

int handle_semaphore_wait(void *key, THREADID tid)
{
    SEMAPHORE_ENTRY * s = get_semaphore_entry(key);
    fail_on_no_semaphore(s, key);

    if(s->value > 0) {
        s->value = s->value - 1;
        return 0;
    }

    THREAD_INFO *t = &all_threads[tid];
    thread_lock(&all_threads[tid]); 

    insert_semaphore_locked(s, t);
    return -1;
}

// Used to debug lock hash states
void print_hash()
{
    MUTEX_ENTRY *s;
    const char *status[] = {"M_LOCKED", "M_UNLOCKED"};

    cerr << "--------- mutex table ---------" << std::endl;
    for(s = mutex_hash; s != NULL; s = (MUTEX_ENTRY *) s->hh.next) {
        cerr << "Key: " << s->key << " - status: " << status[s->status] << std::endl;
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
THREAD_INFO * handle_thread_exit(pthread_t key)
{
    cerr << "[Lock Hash] handle_thread_exit! - key: " << key << std::endl;
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
int handle_reentrant_start(REENTRANT_LOCK *rl, THREADID tid)
{
    THREAD_INFO *t = &all_threads[tid];

    if(rl->busy > 0) {
        thread_lock(t);
        rl->locked = insert(rl->locked, t);
        return 0;
    }

    rl->busy = 1;
    return 1;
}

// handle_reentrant_exit will check if there is
// any thread awaiting to get into the function and
// return it, or null otherwise. 
THREAD_INFO * handle_reentrant_exit(REENTRANT_LOCK *rl, THREADID tid)
{
    if(rl->locked == NULL) {
        rl->busy = 0;
        return NULL;
    }

    thread_unlock(rl->locked, &all_threads[tid]);

    THREAD_INFO * awaked = rl->locked;
    rl->locked = rl->locked->next_lock;
    return awaked;
}
