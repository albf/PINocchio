#include <stdio.h>
#include <iostream>
#include "pin.H"
#include "controller.h"
#include "lockhash.h"

// Global variables used to receive requests
THREAD_INFO * all_threads;
int max_tid;

PIN_MUTEX msg_mutex;
PIN_MUTEX controller_mutex;

MSG msg_buffer;

void fail() {
    PIN_Detach();
}

void controller_init() {
    // Initialize thread information
    all_threads = (THREAD_INFO *) malloc(MAX_THREADS*sizeof(THREAD_INFO));
    max_tid = -1;

    // Initialize thread states information
    max_tid = -1;

    // Initialize thread, msg and read mutex
    for(int i=0; i < MAX_THREADS; i++) {
        all_threads[i].step_status = STEP_MISS;
        all_threads[i].status = UNREGISTERED;

        PIN_MutexInit(&all_threads[i].wait_controller);
        // wait_controllers start locked
        PIN_MutexLock(&all_threads[i].wait_controller);
    }
    PIN_MutexInit(&msg_mutex);
    PIN_MutexInit(&controller_mutex);

    // comm_mutex starts locked
    PIN_MutexLock(&controller_mutex);
}

void send_request(MSG msg) {
    //cerr << "[tid:" << msg.tid << "] sent request: " << msg.msg_type << std::endl;

    // First, lock msg_mutex, will only unlock after controller.
    PIN_MutexLock(&msg_mutex);

    // Update msg buffer and unlock controller.
    msg_buffer = msg;
    PIN_MutexUnlock(&controller_mutex);

    // Wait answer from controller
    PIN_MutexLock(&all_threads[msg.tid].wait_controller);

    // Lastly, unlock msg_mutex allowing other threads to send.
    PIN_MutexUnlock(&msg_mutex);
}

// Returns 1 if can continue, 0 otherwise.
static int can_continue() {
    for(int i = 0; i <= max_tid; i++) {
        if((all_threads[i].step_status != STEP_DONE)
         &&(all_threads[i].status == UNLOCKED)) {
            return 0;
        } 
    }
    return 1;
}

// Release a locked thread waiting for permission
static void release_thread(int tid) {
    // Reset ins_count and update current state
    all_threads[tid].ins_max = INSTRUCTIONS_ON_ROUND;
    all_threads[tid].ins_count = 0;

    // Release thread lock
    PIN_MutexUnlock(&all_threads[tid].wait_controller);

    // Mark step status as missing answer
    all_threads[tid].step_status = STEP_MISS;
}

void controller_main(void * arg) {
    //cerr << "Controller starting" << std::endl;

    while(1) {
        // Wait for a request to come
        PIN_MutexLock(&controller_mutex);

        switch(msg_buffer.msg_type) {
            case MSG_REGISTER:
                // Register arrived with id
                cerr << "[Controller] Received register from: " << msg_buffer.tid << std::endl;

                // Update holder max_tid if a bigger arrived
                if (max_tid < msg_buffer.tid) {
                    max_tid = msg_buffer.tid;
                }

                // Mark as unlocked
                all_threads[msg_buffer.tid].status = UNLOCKED;

                // End request
                PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
                break;
            case MSG_DONE:
                // Thread has finish, mark as step_done! 
                all_threads[msg_buffer.tid].step_status = STEP_DONE;
                break;
            // Mutex events should be treated by lockhash.
            case MSG_BEFORE_LOCK:
                handle_before_lock(msg_buffer.arg, msg_buffer.tid);
                break;

            case MSG_BEFORE_TRY_LOCK:
                handle_before_try(msg_buffer.arg, msg_buffer.tid);
                break;

            case MSG_BEFORE_UNLOCK:
                handle_before_unlock(msg_buffer.arg, msg_buffer.tid);
                break;

            case MSG_AFTER_LOCK:
                handle_after_lock(msg_buffer.arg, msg_buffer.tid);
                break;

            case MSG_AFTER_TRY_LOCK:
                handle_after_try(msg_buffer.arg, msg_buffer.tid);
                break;

            case MSG_AFTER_UNLOCK:
                handle_after_unlock(msg_buffer.arg, msg_buffer.tid);
                break;

        }

        // Check if everyone finished, if yes, release them to run a new step.
        if(can_continue() > 0) {
            for(int i = 0; i <= max_tid; i++) {
                if(all_threads[i].status == UNLOCKED) {
                    release_thread(i);
                }
            }
        }
    }
}
