#include <stdio.h>
#include <iostream>
#include "sync.h"
#include "lockhash.h"
#include "log.h"

// Current thread status
THREAD_INFO *all_threads;
THREADID max_tid;

// Used to only allow one thread to sync
PIN_MUTEX sync_mutex;

// Thread creation structures.
pthread_t pthread_tid;
THREADID pin_tid;
int create_done;

REENTRANT_LOCK create_lock;



void sync_init()
{
    // Initialize thread information, including mutex and initial states
    all_threads = (THREAD_INFO *) malloc(MAX_THREADS * sizeof(THREAD_INFO));
    max_tid = 0;

    for(int i = 0; i < MAX_THREADS; i++) {
        all_threads[i].ins_max = INSTRUCTIONS_ON_ROUND;
        all_threads[i].ins_count = 0;

        all_threads[i].step_status = STEP_MISS;
        all_threads[i].status = UNREGISTERED;
        all_threads[i].create_value = 0;

        // If sleeping, threads should be stopped by the semaphores.
        PIN_SemaphoreInit(&all_threads[i].active);
        PIN_SemaphoreClear(&all_threads[i].active);
    }
    PIN_MutexInit(&sync_mutex);

    // Initialize thread creation structures. Avoids some nasty locks by
    // allowing only one creation per time.
    pthread_tid = 0;
    pin_tid = 0;
    create_done = 0;

    create_lock.busy = 0;
    create_lock.locked = NULL;

    // Lastly, init log structure.
    log_init();
}

// Returns 1 if can continue, 0 otherwise.
static int is_syncronized()
{
    for(UINT32 i = 0; i <= max_tid; i++) {
        if((all_threads[i].step_status != STEP_DONE)
                && (all_threads[i].status == UNLOCKED)) {
            return 0;
        }
    }
    return 1;
}

// Returns 1 if all threads have finished, 0 otherwise.
static int is_finished()
{
    for(UINT32 i = 0; i <= max_tid; i++) {
        if(all_threads[i].status != FINISHED &&
                all_threads[i].status != UNREGISTERED) {
            return 0;
        }
    }
    return 1;
}

// Release a locked thread waiting for permission
void release_thread(THREAD_INFO *ti, INT64 instructions)
{
    // Reset ins_count and update current state
    // Mark step status as missing answer
    ti->ins_max = instructions;
    ti->ins_count = 0;
    ti->step_status = STEP_MISS;

    // Release thread semaphore 
    PIN_SemaphoreSet(&ti->active);
}

// If syncronized, release all unlocked.
void try_release_all()
{
    if(is_syncronized() > 0) {
        log_add();
        for(UINT32 i = 0; i <= max_tid; i++) {
            if((all_threads[i].step_status == STEP_DONE)
                    && (all_threads[i].status == UNLOCKED)) {
                release_thread(&all_threads[i], INSTRUCTIONS_ON_ROUND);
            }
        }
    }
}

void sync(ACTION *action)
{
    // Only one thread should be working at each time.
    PIN_MutexLock(&sync_mutex);

    switch(action->action_type) {
    case ACTION_DONE:
        // Thread has finished one step, mark as done and try to release all
        all_threads[action->tid].step_status = STEP_DONE;
        PIN_SemaphoreClear(&all_threads[action->tid].active);

        try_release_all();
        break;

        // Thread creation works with the following rules:
        // - One thread creating at a time
        // - Create end should wait for starting thread to
        // actually start (pin_create -> ACTION_REGISTER)
        // - thread_tid should come from create_end, as value
        // could change on function. But pointer should be
        // found on start, as it also might be changed.
    case ACTION_REGISTER:
        cerr << "[Sync] Received register from: " << action->tid << std::endl;

        // Update holder max_tid if a bigger arrived
        if(max_tid < action->tid) {
            max_tid = action->tid;
        }

        // Thread starts unlocked
        all_threads[action->tid].status = UNLOCKED;

        // Thread 0 ins't created by pthread_create, but always existed.
        // Don't wait or do any black magic on that regard.
        if (action->tid == 0) {
            release_thread(&all_threads[action->tid], INSTRUCTIONS_ON_ROUND);
            break;
        }

        pin_tid = action->tid;

        // If done > 0, ACTION_AFTER_CREATE already done, must release it,
        // update my own create_value and go on. If not, save pin_tid
        // for later use and mark myself as locked.
        if(create_done > 0) {
            all_threads[pin_tid].create_value = pthread_tid;
            create_done = 0;
            THREAD_INFO * awaked = handle_reentrant_exit(&create_lock);
            if (awaked != NULL) {
                release_thread(awaked, INSTRUCTIONS_ON_ROUND);
            }
        } else {
            create_done = 1;
        }

        release_thread(&all_threads[action->tid], INSTRUCTIONS_ON_ROUND);
        break;

    case ACTION_BEFORE_CREATE:
        // If get locked, try to release everyone active.
        if(handle_reentrant_start(&create_lock, action->tid) == 0) {
            try_release_all();
        }
        break;

    case ACTION_AFTER_CREATE:
        // Save pthread tid and check if ACTION_REGISTER already arrived,
        // if yes, mark pthread_tid and release create_lock
        pthread_tid = ((pthread_t)action->arg.p);

        if(create_done > 0) {
            all_threads[pin_tid].create_value = pthread_tid;
            create_done = 0;
            THREAD_INFO * awaked = handle_reentrant_exit(&create_lock);
            if (awaked != NULL) {
                release_thread(awaked, INSTRUCTIONS_ON_ROUND);
            }
        } else {
            create_done = 1;
        }

        release_thread(&all_threads[action->tid], INSTRUCTIONS_ON_ROUND);
        break;

    case ACTION_FINI:
        cerr << "[Sync] Received fini from: " << action->tid << std::endl;

        // Mark as finished
        all_threads[action->tid].status = FINISHED;
        release_thread(&all_threads[action->tid], INSTRUCTIONS_ON_EXIT);

        // Free any join locked thread, thread 0 shouldn't be joined
        if(action->tid > 0) {
            THREAD_INFO *t = handle_thread_exit(all_threads[action->tid].create_value);
            for(; t != NULL; t = t->next) {
                t->status = UNLOCKED;
                release_thread(t, INSTRUCTIONS_ON_ROUND);
            }
        }

        try_release_all();

        // Check if all threads have finished
        if(is_finished() == 1) {
            cerr << "[Sync] Program finished." << std::endl;
            return;
        }
        break;

    case ACTION_BEFORE_JOIN:
        int allowed;
        allowed = handle_before_join((pthread_t)action->arg.p, action->tid);

        // If not allowed, should await.
        if (allowed == 0) {
            PIN_SemaphoreClear(&all_threads[action->tid].active);
            try_release_all();
        }
        break;

        // Mutex events should be treated by lockhash, unlock thread once done.
    case ACTION_LOCK:
        // If got locked, try to release the others.
        if (handle_lock(action->arg.p, action->tid) > 0) {
            try_release_all();
        }
        break;

    case ACTION_TRY_LOCK:
        // Pass the value back to try_lock function.
        action->arg.i = handle_try_lock(action->arg.p);
        break;

    case ACTION_UNLOCK:
        THREAD_INFO * awaked;
        awaked = handle_unlock(action->arg.p);
        if (awaked != NULL) {
            release_thread(awaked, INSTRUCTIONS_ON_ROUND);
        }
        break;
    }

    // Release sync_mutex so other thread can sync.
    PIN_MutexUnlock(&sync_mutex);

    // Should sleep here if not synced.
    PIN_SemaphoreWait(&all_threads[action->tid].active);
}

void print_threads()
{
    const char *status[] = {"LOCKED", "UNLOCKED", "UNREGISTERED", "FINISHED"};
    const char *step_status[] = {"STEP_MISS", "STEP_DONE"};
    cerr << "--------- thread status ---------" << std::endl;
    for(UINT32 i = 0; i <= max_tid; i++) {
        cerr << "Thread id: " << i << " - status: " << status[all_threads[i].status];
        cerr << " - step status: " << step_status[all_threads[i].step_status];
        cerr << " - create value: " << all_threads[i].create_value << std::endl;
    }
    cerr << "------------------------ " << std::endl;
}
