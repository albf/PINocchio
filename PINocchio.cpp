// Controller related
#include "controller.h"

// Pin related
#include <unistd.h>
#include <iostream>
#include "pin.H"

std::ostream * out = &cerr;

// Might be used to wait for controller
PIN_THREAD_UID controller_tid;

INT32 Usage() {
    cerr << "PINocchio helps (or not) to find if a multithread application scales" << std::endl;
    cerr << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

// Run at thread start
VOID thread_start(THREADID thread_id, CONTEXT *ctxt, INT32 flags, VOID *v) {
    *out << "[PINocchio] Thread Initialized: " << thread_id << std::endl;

    // Check if thread id is under limit
    if(thread_id >= MAX_THREADS) {
        *out << "[PINocchio] Internal error: thread_id not allowed: " << thread_id << std::endl;
         return fail();
    }

    // Send register to controller
    MSG my_msg = {
        .tid = thread_id,
        .msg_type = MSG_REGISTER,
    };
    send_request(my_msg);

    // Send done to controller to get more work
    my_msg.msg_type = MSG_DONE;
    send_request(my_msg);
}

VOID Fini(INT32 code, VOID *v) {
    *out << "===============================================" << std::endl;
    *out << " PINocchio exiting " << std::endl;
    *out << " -- Sync Flushes    : " << thread_holder.sync_flushes << std::endl;
    *out << " -- Delayed Flushes : " << thread_holder.delayed_flushes << std::endl;
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

    // Handler for exit
    PIN_AddFiniFunction(Fini, 0);
 
    // PIN_StartProgram() is not expected to return
    PIN_StartProgram();
    return 0;
}
