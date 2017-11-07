#include <stdio.h>
#include <iostream>
#include "controller.h"
#include "lockhash.h"
#include "log.h"

// Current thread status
THREAD_INFO *all_threads;
THREADID max_tid;

// Used to receive requests
PIN_MUTEX msg_mutex;
PIN_MUTEX controller_mutex;

MSG msg_buffer;

void fail()
{
    PIN_Detach();
}

void controller_init()
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

        PIN_MutexInit(&all_threads[i].wait_controller);
        PIN_MutexLock(&all_threads[i].wait_controller);
        // wait_controllers start locked
    }
    PIN_MutexInit(&msg_mutex);
    PIN_MutexInit(&controller_mutex);
    PIN_MutexLock(&controller_mutex);
    // comm_mutex starts locked

    log_init();
}

void send_request(MSG msg)
{
    // First, lock msg_mutex, will only unlock after controller.
    PIN_MutexLock(&msg_mutex);

    // Update msg buffer and unlock controller.
    msg_buffer = msg;
    PIN_MutexUnlock(&controller_mutex);

    // Wait answer from controller
    PIN_MutexLock(&all_threads[msg.tid].wait_controller);
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
    ti->ins_max = instructions;
    ti->ins_count = 0;

    // Release thread lock
    PIN_MutexUnlock(&ti->wait_controller);

    // Mark step status as missing answer
    ti->step_status = STEP_MISS;
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

void controller_main(void *arg)
{
    pthread_t pthread_tid = 0;
    THREADID pin_tid = 0;
    int create_done = 0;

    REENTRANT_LOCK create_lock = {
        .busy = 0,
        .locked = NULL,
    };

    while(1) {
        // Wait for a request to come
        PIN_MutexLock(&controller_mutex);

        switch(msg_buffer.msg_type) {
        case MSG_DONE:
            // Thread has finished one step, mark as done and try to release all
            all_threads[msg_buffer.tid].step_status = STEP_DONE;
            try_release_all();
            break;

            // Thread creation works with the following rules:
            // - One thread creating at a time
            // - Create end should wait for starting thread to
            // actually start (pin_create -> MSG_REGISTER)
            // - thread_tid should come from create_end, as value
            // could change on function. But pointer should be
            // found on start, as it also might be changed.
        case MSG_REGISTER:
            cerr << "[Controller] Received register from: " << msg_buffer.tid << std::endl;

            // Update holder max_tid if a bigger arrived
            if(max_tid < msg_buffer.tid) {
                max_tid = msg_buffer.tid;
            }

            if(msg_buffer.tid == 0) {
                all_threads[msg_buffer.tid].status = UNLOCKED;
                PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
                break;
            }

            // If done > 0, MSG_AFTER_CREATE already done, must release it,
            // update my own create_value and go on. If not, save pin_tid
            // for later use and mark myself as locked.
            if(create_done > 0) {
                all_threads[msg_buffer.tid].status = UNLOCKED;
                all_threads[msg_buffer.tid].create_value = pthread_tid;
                create_done = 0;
                handle_reentrant_exit(&create_lock);
                all_threads[pin_tid].status = UNLOCKED;
                if(all_threads[pin_tid].step_status == STEP_DONE) {
                    release_thread(&all_threads[pin_tid], INSTRUCTIONS_ON_ROUND);
                }
            } else {
                create_done++;
                all_threads[msg_buffer.tid].status = UNLOCKED;
                pin_tid = msg_buffer.tid;
            }

            // End request
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_BEFORE_CREATE:
            handle_reentrant_start(&create_lock, msg_buffer.tid);
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_AFTER_CREATE:
            // Save pthread tid and check if MSG_REGISTER already arrived,
            // if yes, mark pthread_tid and release create_lock
            pthread_tid = ((pthread_t)msg_buffer.arg);

            if(create_done > 0) {
                all_threads[pin_tid].create_value = pthread_tid;
                create_done = 0;
                handle_reentrant_exit(&create_lock);
            } else {
                create_done++;
                all_threads[msg_buffer.tid].status = LOCKED;
                pin_tid = msg_buffer.tid;
            }

            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_FINI:
            cerr << "[Controller] Received fini from: " << msg_buffer.tid << std::endl;

            // Mark as finished
            all_threads[msg_buffer.tid].status = FINISHED;
            release_thread(&all_threads[msg_buffer.tid], INSTRUCTIONS_ON_EXIT);

            // Free any join locked thread, thread 0 shouldn't be joined
            if(msg_buffer.tid > 0) {
                handle_thread_exit(all_threads[msg_buffer.tid].create_value);
            }

            try_release_all();

            // Check if all threads have finished
            if(is_finished() == 1) {
                cerr << "[Controller] Program finished." << std::endl;
                return;
            }
            break;

        case MSG_BEFORE_JOIN:
            handle_before_join((pthread_t)msg_buffer.arg, msg_buffer.tid);
            break;

            // Mutex events should be treated by lockhash, unlock thread once done.
        case MSG_BEFORE_LOCK:
            handle_before_lock(msg_buffer.arg, msg_buffer.tid);
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_BEFORE_TRY_LOCK:
            handle_before_try(msg_buffer.arg, msg_buffer.tid);
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_BEFORE_UNLOCK:
            handle_before_unlock(msg_buffer.arg, msg_buffer.tid);
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_AFTER_LOCK:
            handle_after_lock(msg_buffer.arg, msg_buffer.tid);
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_AFTER_TRY_LOCK:
            handle_after_try(msg_buffer.arg, msg_buffer.tid);
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;

        case MSG_AFTER_UNLOCK:
            handle_after_unlock(msg_buffer.arg, msg_buffer.tid);
            PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
            break;;
        }

        // Lastly, unlock msg_mutex allowing other threads to send.
        PIN_MutexUnlock(&msg_mutex);
    }
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
