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

HOLDER thread_holder;

void fail() {
    PIN_Detach();
}

void controller_init() {
    // Initialize thread information
    all_threads = (THREAD_INFO *) malloc(MAX_THREADS*sizeof(THREAD_INFO));
    max_tid = -1;

    // Initialize thread states information
    thread_holder.max_tid = -1;
    thread_holder.states = (int *) malloc(MAX_THREADS*sizeof(int));
    thread_holder.sync_flushes = 0;
    thread_holder.delayed_flushes = 0;

    // Initialize thread, msg and read mutex
    for(int i=0; i < MAX_THREADS; i++) {
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

static int get_lower_state() {
    int l = MAX_DELAYS+1;
    for(int i = 0; i < thread_holder.max_tid; i++) {
        if(thread_holder.states[i] < l) {
            l = thread_holder.states[i];
        }
    }
    return l;
}

static void flush() {
    //cerr << "[Controller] Flushing" << std::endl;
    for(int i = 0; i < thread_holder.max_tid; i++) {
        thread_holder.states[i] = 0;
    }
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
                if (max_tid > msg_buffer.tid) {
                    thread_holder.max_tid = msg_buffer.tid;
                    max_tid = msg_buffer.tid;
                }

                // Release thread lock
                PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);
                break;
            case MSG_DONE:
                // Thread has finish, give more work!
                //cerr << "[Controller] Received done from: " << msg_buffer.tid << std::endl;

                // If state 0: business as usual
                if(thread_holder.states[msg_buffer.tid] == 0) {
                    all_threads[msg_buffer.tid].ins_max = INSTRUCTIONS_ON_ROUND;
                }
                // If state >= MAX_DELAYS or lower state > 0 -> flush
                else if (thread_holder.states[msg_buffer.tid] >= MAX_DELAYS) {
                    thread_holder.delayed_flushes++;
                    flush();
                    all_threads[msg_buffer.tid].ins_max = INSTRUCTIONS_ON_ROUND;
                }
                else if (get_lower_state() > 0) {
                    thread_holder.sync_flushes++;
                    flush();
                    all_threads[msg_buffer.tid].ins_max = INSTRUCTIONS_ON_ROUND;
                }
                // If work is done and can't flush, delay
                else {
                    //cerr << "[Controller] Delaying tid: " << msg_buffer.tid << std::endl;
                    all_threads[msg_buffer.tid].ins_max = INSTRUCTIONS_ON_DELAY;
                }

                // Reset ins_count and update current state
                all_threads[msg_buffer.tid].ins_count = 0;
                thread_holder.states[msg_buffer.tid]++;

                // Release thread lock
                PIN_MutexUnlock(&all_threads[msg_buffer.tid].wait_controller);

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
    }
}
