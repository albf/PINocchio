// Controller related
#include "controller.h"

// Pin related
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include "pin.H"

std::ostream * out = &cerr;

// Might be used to wait for controller
PIN_THREAD_UID controller_tid;

INT32 Usage() {
    cerr << "PINocchio helps (or not) to find if a multithread application scales" << std::endl;
    cerr << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

VOID before_mutex_lock(pthread_mutex_t *mutex, THREADID tid) {
    all_threads[tid].holder = mutex;
    MSG msg = {
        .tid = tid,
        .msg_type = MSG_BEFORE_LOCK,
        .arg = (void *) mutex,
    };
    send_request(msg);
    *out << "MUTEX on " << tid << std::endl;
    *out << "before_mutex_lock: " << mutex << std::endl;
}

VOID after_mutex_lock(pthread_mutex_t *mutex, THREADID tid) {
    mutex = all_threads[tid].holder;
    MSG msg = {
        .tid = tid,
        .msg_type = MSG_AFTER_LOCK,
        .arg = (void *) mutex,
    };
    send_request(msg);
    *out << "MUTEX on " << tid << std::endl;
    *out << "after_mutex_lock: " << mutex << std::endl;
}

VOID before_mutex_trylock(pthread_mutex_t *mutex, THREADID tid) {
    all_threads[tid].holder = mutex;
    MSG msg = {
        .tid = tid,
        .msg_type = MSG_BEFORE_TRY_LOCK,
        .arg = (void *) mutex,
    };
    send_request(msg);
    *out << "MUTEX on " << tid << std::endl;
    *out << "before_try_lock: " << mutex << std::endl;
}

VOID after_mutex_trylock(pthread_mutex_t *mutex, THREADID tid) {
    mutex = all_threads[tid].holder;
    MSG msg = {
        .tid = tid,
        .msg_type = MSG_AFTER_TRY_LOCK,
        .arg = (void *) mutex,
    };
    send_request(msg);
    *out << "MUTEX on " << tid << std::endl;
    *out << "after_try_lock: " << mutex << std::endl;
}

VOID before_mutex_unlock(pthread_mutex_t *mutex, THREADID tid) {
    all_threads[tid].holder = mutex;
    MSG msg = {
        .tid = tid,
        .msg_type = MSG_BEFORE_UNLOCK,
        .arg = (void *) mutex,
    };
    send_request(msg);
    *out << "MUTEX on " << tid << std::endl;
    *out << "before_unlock: " << mutex << std::endl;
}

VOID after_mutex_unlock(pthread_mutex_t *mutex, THREADID tid) {
    mutex = all_threads[tid].holder;
    MSG msg = {
        .tid = tid,
        .msg_type = MSG_AFTER_UNLOCK,
        .arg = (void *) mutex,
    };
    send_request(msg);
    *out << "MUTEX on " << tid << std::endl;
    *out << "after_unlock: " << mutex << std::endl;
}

VOID module_load_handler (IMG img, void * v) {
    *out << "module_load_handler" << std::endl;
    if (img == IMG_Invalid()) {
        *out << "ModuleLoadallback received invalid IMG" << std::endl;
        return;
    }

    RTN rtn;

    // Look for pthread_mutex_lock
    rtn = RTN_FindByName(img, "pthread_mutex_lock");
    if (RTN_Valid(rtn)) {
        *out << "Found pthread_mutex_lock on image" << std::endl;
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_mutex_lock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_mutex_lock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);
        *out << "pthread_mutex_lock registered" << std::endl;
    }

    // Look for pthread_mutex_trylock
    rtn = RTN_FindByName(img, "pthread_mutex_trylock");
    if (RTN_Valid(rtn)) {
        *out << "Found pthread_mutex_trylock on image" << std::endl;
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_mutex_trylock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_mutex_trylock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);
        *out << "pthread_mutex_trylock registered" << std::endl;
    }

    // Look for pthread_mutex_unlock
    rtn = RTN_FindByName(img, "pthread_mutex_unlock");
    if (RTN_Valid(rtn)) {
        *out << "Found pthread_mutex_unlock on image" << std::endl;
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_mutex_unlock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_mutex_unlock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);
        *out << "pthread_mutex_unlock registered" << std::endl;
    }
}

// Run at thread start
VOID thread_start(THREADID thread_id, CONTEXT *ctxt, INT32 flags, VOID *v) {
    *out << "[PINocchio] Thread Initialized: " << thread_id << std::endl;

    // Check if thread id is under limit
    if(thread_id >= MAX_THREADS) {
        *out << "[PINocchio] Internal error: thread_id not allowed: " << thread_id << std::endl;
        fail();
    }

    // Send register to controller
    MSG my_msg = {
        .tid = thread_id,
        .msg_type = MSG_REGISTER,
        .arg = NULL,
    };
    send_request(my_msg);

    // Send done to controller to get more work
    my_msg.msg_type = MSG_DONE;
    send_request(my_msg);
}

VOID Fini(INT32 code, VOID *v) {
    *out << "===============================================" << std::endl;
    *out << " PINocchio exiting " << std::endl;
    *out << "===============================================" << std::endl;
}

// Run at instruction start
VOID ins_handler() {
    UINT32 thread_id = PIN_ThreadId();
    THREAD_INFO * my_thread_info = &all_threads[(int)thread_id];
    // Check if finish executing a batch
    if(my_thread_info->ins_count >= my_thread_info->ins_max ) {
        // Send done do controller and wait for a "continue" message
        MSG my_msg = {
            .tid = thread_id,
            .msg_type = MSG_DONE,
        };
        send_request(my_msg);
    }
    my_thread_info->ins_count++; 
}

VOID instruction(INS ins, VOID *v) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_handler, IARG_END);
}

int main(int argc, char *argv[]) {
    cerr <<  "===============================================" << std::endl;
    cerr <<  "Application instrumented by PINocchio" << std::endl;
    cerr <<  "===============================================" << std::endl;

    // Print usage if help was called.
    if(PIN_Init(argc,argv)) {
        return Usage();
    }

    // Initialize controller
    controller_init();

    // Spawn controller
    PIN_SpawnInternalThread(controller_main, 0, 0, &controller_tid);

    // Hadler for instructions
    INS_AddInstrumentFunction(instruction, 0);

    // Handler for thread creation
    PIN_AddThreadStartFunction(thread_start, 0);

    // Handler for mutex functions
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(module_load_handler, NULL);

    // Handler for exit
    PIN_AddFiniFunction(Fini, 0);

    // PIN_StartProgram() is not expected to return
    PIN_StartProgram();
    return 0;
}
