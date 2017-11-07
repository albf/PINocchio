// Controller related
#include "sync.h"
#include "log.h"

// Pin related
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include "pin.H"

std::ostream *out = &cerr;

// Might be used to wait for controller
PIN_THREAD_UID controller_tid;

INT32 Usage()
{
    cerr << "PINocchio helps (or not) to find if a multithread application scales" << std::endl;
    cerr << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

/*
pthread_mutex_lock, pthread_mutex_trylock and pthread_mutex_unlock
will replace the original calls. Create and join follow different
rules and have before and after callbacks.
*/

int hj_pthread_mutex_lock(pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(*out << "mutex_lock called: " << mutex << std::endl);

    ACTION action = {
        .tid = tid,
        .action = ACTION_LOCK,
        .arg = (void *) mutex,
    };
    sync(action);
    return 0;
}

int hj_pthread_mutex_trylock(pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(*out << "mutex_try_lock called: " << mutex << std::endl);

    ACTION action = {
        .tid = tid,
        .action_type = MSG_BEFORE_TRY_LOCK,
        .arg = (void *) mutex,
    };
    sync(action);

    // Try lock result should come as the action arg.
    return action.arg;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(*out << "after_unlock: " << mutex << std::endl);

    ACTION action = {
        .tid = tid,
        .action_type = MSG_AFTER_UNLOCK,
        .arg = (void *) mutex,
    };
    sync(action);
    return 0;
}

VOID before_create(pthread_t *thread, THREADID tid)
{
    DEBUG(*out << "before_create" << std::endl);

    all_threads[tid].holder = (void *) thread;
    ACTION action = {
        .tid = tid,
        .action_type = MSG_BEFORE_CREATE,
    };
    sync(action);
}

VOID after_create(THREADID tid)
{
    DEBUG(*out << "after_create" << std::endl);

    pthread_t *thread = (pthread_t *) all_threads[tid].holder;
    ACTION action = {
        .tid = tid,
        .action_type = MSG_AFTER_CREATE,
        // Save pthread_t parameter as usual, but it's value, not the pointer
        .arg = (void *)(*thread),
    };
    sync(action);
}

VOID before_join(pthread_t thread, THREADID tid)
{
    DEBUG(*out << "before_join" << std::endl);

    ACTION action = {
        .tid = tid,
        .action_type = MSG_BEFORE_JOIN,
        .arg = (void *) thread,
    };
    sync(action);
}

VOID module_load_handler(IMG img, void *v)
{
    DEBUG(*out << "module_load_handler" << std::endl);
    if(img == IMG_Invalid()) {
        *out << "ModuleLoadallback received invalid IMG" << std::endl;
        return;
    }

    RTN rtn;

    // Look for pthread_mutex_lock
    rtn = RTN_FindByName(img, "pthread_mutex_lock");
    if(RTN_Valid(rtn)) {
        DEBUG(*out << "Found pthread_mutex_lock on image" << std::endl);
        RTN_Open(rtn);
/*        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_mutex_lock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_mutex_lock,
                       IARG_THREAD_ID, IARG_END); */
        RTN_Close(rtn);
        DEBUG(*out << "pthread_mutex_lock registered" << std::endl);
    }

    // Look for pthread_mutex_trylock
    rtn = RTN_FindByName(img, "pthread_mutex_trylock");
    if(RTN_Valid(rtn)) {
        DEBUG(*out << "Found pthread_mutex_trylock on image" << std::endl);
        RTN_Open(rtn);
/*        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_mutex_trylock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_mutex_trylock,
                       IARG_THREAD_ID, IARG_END); */
        RTN_Close(rtn);
        DEBUG(*out << "pthread_mutex_trylock registered" << std::endl);
    }

    // Look for pthread_mutex_unlock
    rtn = RTN_FindByName(img, "pthread_mutex_unlock");
    if(RTN_Valid(rtn)) {
        DEBUG(*out << "Found pthread_mutex_unlock on image" << std::endl);
        RTN_Open(rtn);
/*        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_mutex_unlock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_mutex_unlock,
                       IARG_THREAD_ID, IARG_END); */
        RTN_Close(rtn);
        DEBUG(*out << "pthread_mutex_unlock registered" << std::endl);
    }

    // Look for pthread_mutex_unlock
    rtn = RTN_FindByName(img, "pthread_create");
    if(RTN_Valid(rtn)) {
        DEBUG(*out << "Found pthread_create on image" << std::endl);
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_create,
                       IARG_FUNCARG_CALLSITE_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_create,
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);
        DEBUG(*out << "pthread_create registered" << std::endl);
    }

    // Look for pthread_join
    rtn = RTN_FindByName(img, "pthread_join");
    if(RTN_Valid(rtn)) {
        DEBUG(*out << "Found pthread_join on image" << std::endl);
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_join,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);
        DEBUG(*out << "pthread_join registered" << std::endl);
    }
}

VOID thread_start(THREADID thread_id, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    *out << "[PINocchio] Thread Initialized: " << thread_id << std::endl;

    // Check if thread id is under limit
    if(thread_id >= MAX_THREADS) {
        *out << "[PINocchio] Internal error: thread_id not allowed: " << thread_id << std::endl;
        fail();
    }

    // Send register to controller
    ACTION action = {
        .tid = thread_id,
        .action_type = MSG_REGISTER,
        .arg = NULL,
    };
    sync(action);
}

VOID thread_fini(THREADID thread_id, CONTEXT const *ctxt, INT32 flags, VOID *v)
{
    *out << "[PINocchio] Thread Finished: " << thread_id << std::endl;

    ACTION action = {
        .tid = thread_id,
        .action_type = MSG_FINI,
        .arg = NULL,
    };
    sync(action);
}

VOID Fini(INT32 code, VOID *v)
{
    log_dump();
    log_free();
    *out << "===============================================" << std::endl;
    *out << " PINocchio exiting " << std::endl;
    *out << "===============================================" << std::endl;
}

VOID ins_handler()
{
    THREADID thread_id = PIN_ThreadId();
    THREAD_INFO *my_thread_info = &all_threads[(int)thread_id];
    // Check if finish executing a batch
    if(my_thread_info->ins_count >= my_thread_info->ins_max) {
        // Send done do controller and wait for a "continue" message
        ACTION action = {
            .tid = thread_id,
            .action_type = MSG_DONE,
        };
        sync(action);
    }

    my_thread_info->ins_count++;
}

VOID instruction(INS ins, VOID *v)
{
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_handler, IARG_END);
}

int main(int argc, char *argv[])
{
    cerr <<  "===============================================" << std::endl;
    cerr <<  "Application instrumented by PINocchio" << std::endl;
    cerr <<  "===============================================" << std::endl;

    // Print usage if help was called.
    if(PIN_Init(argc, argv)) {
        return Usage();
    }

    // Initialize sync structure 
    sync_init();

    // Spawn controller
    PIN_SpawnInternalThread(controller_main, 0, 0, &controller_tid);

    // Hadler for instructions
    INS_AddInstrumentFunction(instruction, 0);

    // Handler for thread creation
    PIN_AddThreadStartFunction(thread_start, 0);

    // Handler for thread Fini
    PIN_AddThreadFiniFunction(thread_fini, 0);

    // Handler for mutex functions
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(module_load_handler, NULL);

    // Handler for exit
    PIN_AddFiniFunction(Fini, 0);

    // PIN_StartProgram() is not expected to return
    PIN_StartProgram();
    return 0;
}
