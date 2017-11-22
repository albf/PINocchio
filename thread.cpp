#include <stdio.h>
#include <iostream>
#include "thread.h"
#include "trace_bank.h"
#include "error.h"
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
    DEBUG(cerr << "[Thread] Threads structure initialized" << std::endl);
}

// Returns:  1 if all threads have finished,
//           0 otherwise.
int thread_all_finished()
{
    if (exec_track_is_empty() == 0) {
        return 0;
    }

    // It's finished, check if it's deadlocked.
    for(int i = 0; i < MAX_THREADS; i++) {
        if(all_threads[i].status == UNLOCKED) {
            cerr << "[PINocchio] Internal Error: exec_track says it's empty but a thread is running: ";
            cerr << "i" << std::endl;
        } else if (all_threads[i].status == LOCKED) {
            cerr << "[PINocchio] Error: Deadlock on thread ";
            cerr << "i" << std::endl;
        }
    }

    return 1;
}

// Check what are the threads that can be released.
void thread_try_release_all()
{
    // In other words: keep trying to start threads until it's not possible. 
    for (THREAD_INFO *t = exec_tracker_awake(); t != NULL; t = exec_tracker_awake()) {
        // Release thread semaphore, that's all required to let thread continue.
        PIN_SemaphoreSet(&t->active);
    }
}

void thread_start(THREAD_INFO *target, THREAD_INFO *creator)
{
    // Update holder max_tid if a bigger arrived
    if(max_tid < target->pin_tid) {
        max_tid = target->pin_tid;
    }

    // Thread 0 is special, will pass NULL and starts with 0.
    // Other threads should start running and should be awake at first round.
    target->ins_count = creator != NULL ? (creator->ins_count-1) : 0;
    target->status = UNLOCKED;
    trace_bank_register(target->pin_tid, target->ins_count);

    // Thread start running or a deadlock might happen.
    // If, for some reason, there is someone really advanced, next sync
    // will stop anything important.
    exec_tracker_plus();
    PIN_SemaphoreSet(&target->active);
}

void thread_finish(THREAD_INFO *target)
{
    target->status = FINISHED;
    trace_bank_finish(target->pin_tid, target->ins_count);

    exec_tracker_minus();
}

// Should:
// - Lock the semaphore. It will be awake, so it requires to be locked.
// - Update trace bank, it's a change on thread state.
// - Update exec_tracker running counter. 
void thread_lock(THREAD_INFO *target)
{
    PIN_SemaphoreClear(&target->active);

    target->status = LOCKED;
    trace_bank_update(target->pin_tid, target->ins_count, LOCKED);

    exec_tracker_minus();
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

// It has advanced if exec_tracker ins_max has changed
int thread_has_advanced() {
    return exec_tracker_changed();
}

void print_threads()
{
    const char *status[] = {"UNLOCKED", "LOCKED", "UNREGISTERED", "FINISHED"};
    cerr << "--------- thread status ---------" << std::endl;
    for(UINT32 i = 0; i <= max_tid; i++) {
        cerr << "Thread id: " << i << std::endl;
        cerr << " -       status: " << status[all_threads[i].status] << std::endl;
        cerr << " - create_value: " << all_threads[i].create_value << std::endl;
        cerr << " -    ins_count: " << all_threads[i].ins_count << std::endl;
        cerr << " -       active: " << PIN_SemaphoreIsSet(&all_threads[i].active) << std::endl;
    }
    cerr << "------------------------ " << std::endl;
}
