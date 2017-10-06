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

void insert_about_try(MUTEX_ENTRY *mutex, THREAD_INFO *entry)
{
    mutex->about_try = insert(mutex->about_try, entry);
}

void insert_about_unlock(MUTEX_ENTRY *mutex, THREAD_INFO *entry)
{
    mutex->about_unlock = insert(mutex->about_unlock, entry);
}

void handle_before_lock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);
    THREAD_INFO *t;

    switch(s->status) {
        // If locked, locking, unlocking or waiting, just insert on list and mark as thread as locked.
    case M_LOCKED:
    case M_LOCKING:
    case M_UNLOCKING:
    case M_WAITING:
        t = &all_threads[tid];
        t->status = LOCKED;
        try_release_all();
        insert_locked(s, t);
        break;
        // If unlocked, first to come, just start locking.
    case M_UNLOCKED:
        s->status = M_LOCKING;
        break;
    }
}

void handle_after_lock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);

    // Should always be on status locking when handle_lock_end is called.
    // If no one is already waiting to unlock, change to locked and free
    // about_try threads, if any.

    if(s->about_unlock != NULL) {
        s->status = M_UNLOCKING;
        s->about_unlock->status = UNLOCKED;
        release_thread(s->about_unlock, INSTRUCTIONS_ON_ROUND);
        s->about_unlock = s->about_unlock->next;
    } else {
        s->status = M_LOCKED;
        if(s->about_try != NULL) {
            THREAD_INFO *t;
            for(t = s->about_try; t != NULL; t = t->next) {
                s->threads_trying++;
                t->status = UNLOCKED;
                release_thread(t, INSTRUCTIONS_ON_ROUND);
            }
            s->about_try = NULL;
        }
    }
}

void handle_before_try(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);
    THREAD_INFO *t = &all_threads[tid];

    switch(s->status) {
        // If locked, just go and update trying counter.
    case M_LOCKED:
        s->threads_trying++;
        break;
        // If waiting/unlocking, mutex will unlock. Since try could
        // be the first one, check if there is anyone already locked,
        // if no, add try as lock (which will succeed), if not, do
        // the same as locking: just add on about_try.
    case M_UNLOCKING:
    case M_WAITING:
        t->status = LOCKED;
        try_release_all();
        if(s->locked == NULL) {
            insert_locked(s, t);
        } else {
            insert_about_try(s, t);
        }
        // If locking, must wait for it to finish, add to list.
    case M_LOCKING:
        t->status = LOCKED;
        try_release_all();
        insert_about_try(s, t);
        break;
        // If unlocked, just lock, no one is trying.
    case M_UNLOCKED:
        s->status = M_LOCKING;
        break;
    }
}

void handle_after_try(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);

    switch(s->status) {
        // If locked, must update threads_trying track
    case M_LOCKED:
        s->threads_trying--;
        break;
        // If waiting, was locked and now should check if could continue
        // thread blocked on unlock.
    case M_WAITING:
        s->threads_trying--;
        if(s->threads_trying == 0) {
            s->status = M_UNLOCKING;
            s->about_unlock->status = UNLOCKED;
            release_thread(s->about_unlock, INSTRUCTIONS_ON_ROUND);
            s->about_unlock = s->about_unlock->next;
        }
        break;;
        // If finished try, could be actually locking, mark as locked.
    case M_LOCKING:
        if(s->about_unlock != NULL) {
            s->status = M_UNLOCKING;
            s->about_unlock->status = UNLOCKED;
            release_thread(s->about_unlock, INSTRUCTIONS_ON_ROUND);
            s->about_unlock = s->about_unlock->next;
        } else {
            s->status = M_LOCKED;
        }
        break;
    case M_UNLOCKED:
    case M_UNLOCKING:
        // M_UNLOCKED: shouldn't happen.
        // M_UNLOCKING: shouldn't happen.
        break;
    }
}


void handle_before_unlock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);
    THREAD_INFO *t = &all_threads[tid];

    switch(s->status) {
        // If currently locked, check for threads trying, if anyone,
        // waits, if not, just mark as free. Free shouldn't being
        // unlock, but just let it do what it wants.
    case M_LOCKED:
    case M_UNLOCKED:
        if(s->threads_trying > 0) {
            s->status = M_WAITING;
            t->status = LOCKED;
            try_release_all();
            insert_about_unlock(s, t);
        } else {
            s->status = M_UNLOCKING;
        }
        break;
        // Assumes as correct, if someone is locking, it will immediatly
        // becomes free. Wait for it to finish locking to unlock.
    case M_LOCKING:
        s->status = M_WAITING;
        insert_about_unlock(s, t);
        break;
        // If free, waiting or unlocking, something is wrong: shouldn't unlock
        // on these states. Let it go, assumes nothing let it try to unlock.
    case M_UNLOCKING:
        insert_about_unlock(s, t);
        break;
    case M_WAITING:
        insert_about_unlock(s, t);
        break;
    }
}

void handle_after_unlock(void *key, THREADID tid)
{
    MUTEX_ENTRY *s = get_entry(key);

    // Unlock should always end on the UNLOCKING state.
    // Check if anyone waiting to unlock or lock, if it is, let it continue.
    // If not, just mark as free.
    if(s->about_unlock != NULL) {
        s->about_unlock->status = UNLOCKED;
        release_thread(s->about_unlock, INSTRUCTIONS_ON_ROUND);
        s->about_unlock = s->about_unlock->next;
    } else if(s->locked != NULL) {
        s->status = M_LOCKING;
        s->locked->status = UNLOCKED;
        release_thread(s->locked, INSTRUCTIONS_ON_ROUND);
        s->locked = s->locked->next;
    } else {
        // Don't have to check for try_lock, if a first try lock arrived,
        // it will go to lock. If a second arrived, first condition will
        // already enter.
        s->status = M_UNLOCKED;
    }
}

// Used to debug lock hash states
void print_hash()
{
    MUTEX_ENTRY *s;
    const char *status[] = {"M_LOCKED", "M_UNLOCKING", "M_UNLOCKED", "M_UNLOCKING", "M_WAITING"};

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
