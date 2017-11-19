#include <stdio.h>
#include <iostream>
#include "sync.h"
#include "lock_hash.h"
#include "thread.h"

// Used to only allow one thread to sync
PIN_MUTEX sync_mutex;

// Thread creation structures.
pthread_t pthread_tid;
THREADID pin_tid;
int create_done;
THREADID creator_pin_tid;

REENTRANT_LOCK create_lock;



void sync_init()
{
    PIN_MutexInit(&sync_mutex);

    // Initialize thread creation structures. Avoids some nasty locks by
    // allowing only one creation per time.
    pthread_tid = 0;
    pin_tid = 0;
    create_done = 0;

    create_lock.busy = 0;
    create_lock.locked = NULL;

    // Lastly, init thread, trace bank and exec tracker structures.
    thread_init();
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

        thread_try_release_all();
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

        // Thread starts unlocked
        thread_start(&all_threads[action->tid], &all_threads[creator_pin_tid]);

        // Thread 0 ins't created by pthread_create, but always existed.
        // Don't wait or do any black magic on that regard.
        if (action->tid > 0) {
            pin_tid = action->tid;
            // If done > 0, ACTION_AFTER_CREATE already done, must release it,
            // update my own create_value and go on. If not, save pin_tid
            // for later use and mark myself as locked.
            if(create_done > 0) {
                all_threads[pin_tid].create_value = pthread_tid;
                create_done = 0;
                THREAD_INFO * awaked = handle_reentrant_exit(&create_lock, action->tid);
                if (awaked != NULL) {
                    thread_try_release_all();
                }
            } else {
                create_done = 1;
            }
        }

        thread_try_release_all();
        break;

    case ACTION_BEFORE_CREATE:
        // If get locked, try to release everyone active.
        if(handle_reentrant_start(&create_lock, action->tid) == 0) {
            thread_try_release_all();
        }

        // Save current instruction count from thread creator.
        creator_pin_tid = action->tid;
        break;

    case ACTION_AFTER_CREATE:
        // Save pthread tid and check if ACTION_REGISTER already arrived,
        // if yes, mark pthread_tid and release create_lock
        pthread_tid = ((pthread_t)action->arg.p);

        if(create_done > 0) {
            all_threads[pin_tid].create_value = pthread_tid;
            create_done = 0;
            THREAD_INFO * awaked = handle_reentrant_exit(&create_lock, action->tid);
            if (awaked != NULL) {
                thread_try_release_all();
            }
        } else {
            create_done = 1;
        }

        thread_try_release_all();
        break;

    case ACTION_FINI:
        cerr << "[Sync] Received fini from: " << action->tid << std::endl;

        // Mark as finished
        thread_finish(&all_threads[action->tid]);

        // Free any join locked thread, thread 0 shouldn't be joined
        if(action->tid > 0) {
            THREAD_INFO *t = handle_thread_exit(all_threads[action->tid].create_value);
            for(; t != NULL; t = t->next_lock) {
                thread_unlock(t, &all_threads[action->tid]);
            }
        }

        // Check if all threads have finished
        if(thread_all_finished() == 1) {
            cerr << "[Sync] Program finished." << std::endl;
            return;
        }

        thread_try_release_all();
        break;

    case ACTION_BEFORE_JOIN:
        int allowed;
        allowed = handle_before_join((pthread_t)action->arg.p, action->tid);

        // If not allowed, should await.
        if (allowed == 0) {
            PIN_SemaphoreClear(&all_threads[action->tid].active);
            thread_try_release_all();
        }
        break;


        // Mutex events should be treated by lock_hash, unlock thread once done.
    case ACTION_LOCK_DESTROY:
        handle_lock_init(action->arg.p); 
        break;

    case ACTION_LOCK_INIT:
        handle_lock_init(action->arg.p);
        break;

    case ACTION_LOCK:
        // If got locked, try to release the others.
        if (handle_lock(action->arg.p, action->tid) > 0) {
            thread_try_release_all();
        }
        break;

    case ACTION_TRY_LOCK:
        // Pass the value back to try_lock function.
        action->arg.i = handle_try_lock(action->arg.p);
        break;

    case ACTION_UNLOCK:
        THREAD_INFO * awaked;
        awaked = handle_unlock(action->arg.p, action->tid);
        if (awaked != NULL) {
            thread_try_release_all();
        }
        break;

    case ACTION_SEM_DESTROY:
        handle_semaphore_destroy(action->arg.p);
        break;

    case ACTION_SEM_GETVALUE:
        action->arg.i = handle_semaphore_getvalue(action->arg.p);
        break;

    case ACTION_SEM_INIT:
        handle_semaphore_init(action->arg.p, action->arg.i);
        break;

    case ACTION_SEM_POST:
        awaked = handle_semaphore_post(action->arg.p, action->tid);
        thread_try_release_all();
        break;

    case ACTION_SEM_TRYWAIT:
        // Pass the value back to sem_trywait function.
        action->arg.i = handle_semaphore_trywait(action->arg.p);
        break;

    case ACTION_SEM_WAIT:
        // If got locked, try to release the others.
        if (handle_semaphore_wait(action->arg.p, action->tid) >= 0) {
            thread_try_release_all();
        }
        break;
    }

    // Release sync_mutex so other thread can sync.
    PIN_MutexUnlock(&sync_mutex);

    // Should sleep here if not synced.
    PIN_SemaphoreWait(&all_threads[action->tid].active);
}
