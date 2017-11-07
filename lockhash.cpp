#include <iostream>
#include "lockhash.h"
#include "pin.H"

MUTEX_ENTRY *mutex_hash = NULL;

// get_entry will find a given entry or, if doesn't exist, create one.
MUTEX_ENTRY *get_entry(void *key)
{
    MUTEX_ENTRY *s;

    HASH_FIND_PTR(mutex_hash, &key, s);
    if(s) {
        return s;
    }

    // Not found, add new and return it.
    s = (MUTEX_ENTRY *) malloc(sizeof(MUTEX_ENTRY));
    s->key = key;
    s->threads_trying = 0;
    s->status = M_UNLOCKED;

    s->locked = NULL;
    s->about_try = NULL;

    HASH_ADD_PTR(mutex_hash, key, s);
    return s;
}

// Insert a given THREAD_INFO on a linked list. Returns the new pointer
// to the list head.
THREAD_INFO *insert(THREAD_INFO *list, THREAD_INFO *entry)
{
    entry->next = NULL;

    // Unique entry on the moment
    if(list == NULL) {
        return entry;
    }

    // Add on the end of the list
    THREAD_INFO *t;
    for(t = list; t->next != NULL; t = t->next);
    t->next = entry;
    return list;
}

void insert_locked(MUTEX_ENTRY *mutex, THREAD_INFO *entry)
{
    mutex->locked = insert(mutex->locked, entry);
}

void handle_lock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);

    // If locked, just insert on list and mark as thread as locked.
    if (s->status == M_LOCKED) {
        THREAD_INFO *t = &all_threads[tid];
        t->status = LOCKED;
        try_release_all();
        insert_locked(s, t);
        return;
    }

    // If unlocked, first to come, just start locking.
    s->status = M_LOCKED;
    return;
}

int handle_try_lock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);

    if (s->status == M_UNLOCKED) {
        s->status = M_LOCKED;
        return 0;
    }
    return 1;
}

void handle_unlock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);
    THREAD_INFO *t = &all_threads[tid];

    if(s->locked != NULL) {
        s->status = M_LOCKED;
        s->locked->status = UNLOCKED;
        release_thread(s->locked, INSTRUCTIONS_ON_ROUND);
        s->locked = s->locked->next;
    } else {
        s->status = M_UNLOCKED;
    }
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

JOIN_ENTRY *join_hash = NULL;

// get_entry will find a given entry or, if doesn't exist, create one.
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
// Also check for locked on join threads, release
// if any.
void handle_thread_exit(pthread_t key)
{
    JOIN_ENTRY *s = get_join_entry(key);
    s->allow = 1;

    THREAD_INFO *t;
    for(t = s->locked; t != NULL; t = t->next) {
        t->status = UNLOCKED;
        release_thread(t, INSTRUCTIONS_ON_ROUND);
    }
}

// handle_before_join deals with join request. If
// allow, just release thread as usual. If not,
// lock on thread exitence and check if others
// running threads could continue.
void handle_before_join(pthread_t key, THREADID tid)
{
    JOIN_ENTRY *s = get_join_entry(key);
    THREAD_INFO *t = &all_threads[tid];

    if(s->allow == 0) {
        t->status = LOCKED;
        s->locked = insert(s->locked, t);
        try_release_all();
    } else {
        release_thread(t, INSTRUCTIONS_ON_ROUND);
    }
}

// handle_reentrant_start will deal with the case
// of a starting reentrat function that the tool
// wants to lock.
void handle_reentrant_start(REENTRANT_LOCK *rl, THREADID tid)
{
    THREAD_INFO *t = &all_threads[tid];

    if(rl->busy > 0) {
        t->status = LOCKED;
        rl->locked = insert(rl->locked, t);
        try_release_all();
    } else {
        rl->busy = 1;
    }
}

// handle_reentrant_exit will let other thread
// access the reentrant function.
void handle_reentrant_exit(REENTRANT_LOCK *rl)
{
    if(rl->locked == NULL) {
        rl->busy = 0;
    } else {
        rl->locked->status = UNLOCKED;
        release_thread(rl->locked, INSTRUCTIONS_ON_ROUND);
        rl->locked = rl->locked->next;
    }
}
