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
    s->status = M_UNLOCKED;
    s->locked = NULL;

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

int handle_lock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);

    if (s->status == M_UNLOCKED) {
        // If unlocked, first to come, just start locking.
        s->status = M_LOCKED;
        return 0;
    }

    // If locked, just insert on list and mark as thread as locked.
    THREAD_INFO *t = &all_threads[tid];
    t->status = LOCKED;
    insert_locked(s, t);
    return 1;
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

// Handle_unlock returns the data from the awaked thread.
// If none was awaked, just return NULL.
THREAD_INFO * handle_unlock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);

    if(s->locked != NULL) {
        s->status = M_LOCKED;
        s->locked->status = UNLOCKED;

        THREAD_INFO * awaked = s->locked;
        s->locked = s->locked->next;
        return awaked;
    } else {
        // Odd case, won't change anything.
        s->status = M_UNLOCKED;
        return NULL; 
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
// Return the list of locked threads on join.
THREAD_INFO * handle_thread_exit(pthread_t key)
{
    cerr << "[lockhash] handle_thread_exit! - key: " << key << std::endl;
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
    cerr << "[lockhash] before_join! - " << tid << std::endl;
    cerr << "[lockhash] before_join! - key: " << key << std::endl;
    JOIN_ENTRY *s = get_join_entry(key);
    THREAD_INFO *t = &all_threads[tid];

    if(s->allow == 0) {
        cerr << "[lockhash] s->locked! - " << s->locked << std::endl;
        t->status = LOCKED;
        s->locked = insert(s->locked, t);
        cerr << "[lockhash] s->locked! - " << s->locked << std::endl;
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
        t->status = LOCKED;
        rl->locked = insert(rl->locked, t);
        return 0;
    }

    rl->busy = 1;
    return 1;
}

// handle_reentrant_exit will check if there is
// any thread awaiting to get into the function and
// return it, or null otherwise. 
THREAD_INFO * handle_reentrant_exit(REENTRANT_LOCK *rl)
{
    if(rl->locked == NULL) {
        rl->busy = 0;
        return NULL;
    }

    rl->locked->status = UNLOCKED;
    THREAD_INFO * awaked = rl->locked;
    rl->locked = rl->locked->next;
    return awaked;
}
