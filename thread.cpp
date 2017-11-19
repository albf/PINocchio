#include <stdio.h>
#include <iostream>
#include "thread.h"
#include "trace_bank.h"
#include "exec_tracker.h"

// Current thread status
THREAD_INFO *all_threads;
THREADID max_tid;

void thread_init()
{
    // Initialize thread information, including mutex and initial states
    all_threads = (THREAD_INFO *) malloc(MAX_THREADS * sizeof(THREAD_INFO));
    max_tid = 0;

    for(int i = 0; i < MAX_THREADS; i++) {
        all_threads[i].ins_count = 0;
        all_threads[i].pin_tid = i;

        all_threads[i].status = UNREGISTERED;
        all_threads[i].create_value = 0;

        // If sleeping, threads should be stopped by the semaphores.
        PIN_SemaphoreInit(&all_threads[i].active);
        PIN_SemaphoreClear(&all_threads[i].active);
    }

    trace_bank_init();
    exec_tracker_init();
    cerr << "[Thread] Threads structure initialized" << std::endl;
}

// Returns 1 if all threads have finished, 0 otherwise.
int thread_all_finished()
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
void release_thread(THREAD_INFO *ti)
{
    // Release thread semaphore 
    PIN_SemaphoreSet(&ti->active);
}

// Check what are the threads that can be released.
void thread_try_release_all()
{
    THREAD_INFO *r = exec_tracker_peek_running();
    THREAD_INFO *w = exec_tracker_peek_waiting();

    // In other words: while there is someone waiting and:
    // - There is no running thread.
    // - If there is, the one more advanced is before the more late thread.
    while(w != NULL && (r == NULL || w ->ins_count < r ->ins_count)) {
        exec_tracker_awake();
        release_thread(w);

        r = exec_tracker_peek_waiting();
        w = exec_tracker_peek_waiting();
    }
}

void thread_start(THREAD_INFO *target, THREAD_INFO *creator)
{
    // Update holder max_tid if a bigger arrived
    if(max_tid < target->pin_tid) {
        max_tid = target->pin_tid;
    }

    target->status = UNLOCKED;
    trace_bank_register(target->pin_tid, creator->ins_count);

    exec_tracker_insert(target);
}

void thread_finish(THREAD_INFO *target)
{
    target->status = FINISHED;
    trace_bank_finish(target->pin_tid, target->ins_count);

    exec_tracker_remove(target);
}

void thread_lock(THREAD_INFO *target)
{
    target->status = LOCKED;
    trace_bank_update(target->pin_tid, target->ins_count, LOCKED);

    exec_tracker_remove(target);
}

void thread_unlock(THREAD_INFO *target, THREAD_INFO *unlocker)
{
    target->status = UNLOCKED;
    target->ins_count = unlocker->ins_count;
    trace_bank_update(target->pin_tid, target->ins_count, UNLOCKED);
    
    exec_tracker_insert(target);
}

void thread_sleep(THREAD_INFO *target)
{
    PIN_SemaphoreClear(&target->active);
    exec_tracker_sleep(target);
}

void print_threads()
{
    const char *status[] = {"UNLOCKED", "LOCKED", "UNREGISTERED", "FINISHED"};
    cerr << "--------- thread status ---------" << std::endl;
    for(UINT32 i = 0; i <= max_tid; i++) {
        cerr << "Thread id: " << i << " - status: " << status[all_threads[i].status];
        cerr << " - create value: " << all_threads[i].create_value << std::endl;
    }
    cerr << "------------------------ " << std::endl;
}
