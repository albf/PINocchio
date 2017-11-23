// Sync related
#include "sync.h"
#include "trace_bank.h"

// Pin related
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include "error.h"
#include "knob.h"
#include "pin.H"



/*
pthread_mutex_lock, pthread_mutex_trylock, pthread_mutex_unlock
sem_destroy, sem_getvalue, sem_init, sem_post, sem_trywait and sem_wait
will have their original calls replaced. Create and join follow different
rules, create need both before and after callbacks.
*/

/* Mutex hijackers */

int hj_pthread_mutex_destroy(pthread_mutex_t *mutex, THREADID tid) {
    DEBUG(cerr << "mutex_destroy called: " << mutex << std::endl);

    ACTION action = {
        tid,
        ACTION_LOCK_DESTROY,
        {(void *) mutex},
    };
    sync(&action);

    return 0;
}

// hj_pthread_mutex_init is not accepting to receive THREADID tid. Avoiding it for now.
int hj_pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t attr, THREADID tid)
{
    DEBUG(cerr << "mutex_init called: " << mutex << " by " << tid << std::endl);

    ACTION action = {
        tid,
        ACTION_LOCK_INIT,
        {(void *) mutex},
    };
    sync(&action);

    return 0;
}

int hj_pthread_mutex_lock(pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(cerr << "mutex_lock called: " << mutex << std::endl);

    ACTION action = {
        tid,
        ACTION_LOCK,
        {(void *) mutex},
    };
    sync(&action);
    return 0;
}

int hj_pthread_mutex_trylock(pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(cerr << "mutex_try_lock called: " << mutex << std::endl);

    ACTION action = {
        tid,
        ACTION_TRY_LOCK,
        {(void *) mutex},
    };
    sync(&action);

    // Try lock result should come as the action arg.
    return action.arg.i;
}

int hj_pthread_mutex_unlock(pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(cerr << "mutex_unlock called: " << mutex << std::endl);

    ACTION action = {
        tid,
        ACTION_UNLOCK,
        {(void *) mutex},
    };
    sync(&action);
    return 0;
}

/* Semaphore Hijackers */

int hj_sem_destroy(sem_t *sem, THREADID tid) {
    DEBUG(cerr << "sem_destroy called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_DESTROY,
        {(void *) sem},
    };
    sync(&action);

    return 0;
}

int hj_sem_getvalue(sem_t *sem, int *value, THREADID tid) {
    DEBUG(cerr << "sem_getvalue called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_GETVALUE,
        {(void *) sem},
    };
    sync(&action);

    return action.arg.i;
}

// hj_sem_init is not accepting to receive THREADID tid. Avoiding it for now.
int hj_sem_init(sem_t *sem, int pshared, unsigned int value, THREADID tid)
{
    DEBUG(cerr << "sem_init called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_INIT,
        {(void *) sem, NULL, (int) value},
    };
    sync(&action);

    return 0;
}

int hj_sem_post(sem_t *sem, THREADID tid) {
    DEBUG(cerr << "sem_post called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_POST,
        {(void *) sem},
    };
    sync(&action);

    return 0;
}

int hj_sem_trywait(sem_t *sem, THREADID tid) {
    DEBUG(cerr << "sem_trywait called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_TRYWAIT,
        {(void *) sem},
    };
    sync(&action);

    return action.arg.i;
}

int hj_sem_wait(sem_t *sem, THREADID tid) {
    DEBUG(cerr << "sem_wait called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_WAIT,
        {(void *) sem},
    };
    sync(&action);

    return 0;
}

/* Conditional Variables Hijackers  */

int hj_pthread_cond_broadcast(pthread_cond_t *cond, THREADID tid) {
    DEBUG(cerr << "pthread_cond_broadcast called: " << cond << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_BROADCAST,
        {(void *) cond},
    };
    sync(&action);

    return 0;
}

int hj_pthread_cond_destroy(pthread_cond_t *cond, THREADID tid) {
    DEBUG(cerr << "pthread_cond_destroy called: " << cond << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_DESTROY,
        {(void *) cond},
    };
    sync(&action);

    return 0;
}

// hj_pthread_cond_init is not accepting to receive THREADID tid. Avoiding it for now.
int hj_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr, THREADID tid) {
    DEBUG(cerr << "pthread_cond_init called: " << cond << std::endl);

    if (attr != NULL) {
        cerr << "Error: pthread_cond_init attr should be NULL on tid: " << tid << std::endl;
        fail();
    }

    ACTION action = {
        tid,
        ACTION_COND_INIT,
        {(void *) cond},
    };
    sync(&action);

    return 0;
}

int hj_pthread_cond_signal(pthread_cond_t *cond, THREADID tid) {
    DEBUG(cerr << "pthread_cond_signal called: " << cond << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_SIGNAL,
        {(void *) cond},
    };
    sync(&action);

    return 0;
}

int hj_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, THREADID tid) {
    DEBUG(cerr << "pthread_cond_wait called: " << cond << ", " << mutex << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_WAIT,
        {(void *) cond, (void*) mutex},
    };
    sync(&action);

    return 0;
}

/* Create/Join callbacks */

VOID before_create(pthread_t *thread, THREADID tid)
{
    DEBUG(cerr << "before_create" << std::endl);

    all_threads[tid].holder = (void *) thread;
    ACTION action = {
        tid,
        ACTION_BEFORE_CREATE,
    };
    sync(&action);
}

VOID after_create(THREADID tid)
{
    DEBUG(cerr << "after_create" << std::endl);

    pthread_t *thread = (pthread_t *) all_threads[tid].holder;
    ACTION action = {
        tid,
        ACTION_AFTER_CREATE,
        // Save pthread_t parameter as usual, but it's value, not the pointer
        {(void *)(*thread)},
    };
    sync(&action);
}

int hj_pthread_join(pthread_t thread, THREADID tid)
{
    DEBUG(cerr << "before_join" << std::endl);

    ACTION action = {
        tid,
        ACTION_BEFORE_JOIN,
        {(void *) thread},
    };
    sync(&action);
    return 0;
}

VOID module_load_handler(IMG img, void *v)
{
    DEBUG(cerr << "module_load_handler" << std::endl);
    if(img == IMG_Invalid()) {
        cerr << "[PINocchio] Internal Error: ModuleLoadallback received invalid IMG" << std::endl;
        return;
    }

    RTN rtn;

    // Look for pthread_mutex_init and hijack it
    rtn = RTN_FindByName(img, "pthread_mutex_init");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_init on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_mutex_init,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_init hijacked" << std::endl);
    }

    // Look for pthread_mutex_destroy and hijack it
    rtn = RTN_FindByName(img, "pthread_mutex_destroy");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_destroy on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_mutex_destroy,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_destroy hijacked" << std::endl);
    }

    // Look for pthread_mutex_lock and hijack it
    rtn = RTN_FindByName(img, "pthread_mutex_lock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_lock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_mutex_lock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_lock hijacked" << std::endl);
    }

    // Look for pthread_mutex_trylock and hijack it
    rtn = RTN_FindByName(img, "pthread_mutex_trylock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_trylock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_mutex_trylock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_trylock hijacked" << std::endl);
    }

    // Look for pthread_mutex_unlock and hijack it
    rtn = RTN_FindByName(img, "pthread_mutex_unlock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_unlock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_mutex_unlock,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_unlock hijacked" << std::endl);
    }

    // Look for sem_destroy and hijack it
    rtn = RTN_FindByName(img, "sem_destroy");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_destroy on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_sem_destroy,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_destroy hijacked" << std::endl);
    }

    // Look for sem_getvalue and hijack it
    rtn = RTN_FindByName(img, "sem_getvalue");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_getvalue on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_sem_getvalue,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_getvalue hijacked" << std::endl);
    }

    // Look for sem_init and hijack it
    rtn = RTN_FindByName(img, "sem_init");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_init on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_sem_init,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_init hijacked" << std::endl);
    }

    // Look for sem_post and hijack it
    rtn = RTN_FindByName(img, "sem_post");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_post on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_sem_post,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_post hijacked" << std::endl);
    }

    // Look for sem_trywait and hijack it
    rtn = RTN_FindByName(img, "sem_trywait");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_trywait on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_sem_trywait,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_trywait hijacked" << std::endl);
    }

    // Look for sem_wait and hijack it
    rtn = RTN_FindByName(img, "sem_wait");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_wait on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_sem_wait,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_wait hijacked" << std::endl);
    }

    // Look for pthread_cond_broadcast and hijack it
    rtn = RTN_FindByName(img, "pthread_cond_broadcast");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_broadcast on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_cond_broadcast,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_broadcast hijacked" << std::endl);
    }

    // Look for pthread_cond_destroy and hijack it
    rtn = RTN_FindByName(img, "pthread_cond_destroy");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_destroy on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_cond_destroy,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_destroy hijacked" << std::endl);
    }

    // Look for pthread_cond_init and hijack it
    rtn = RTN_FindByName(img, "pthread_cond_init");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_init on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_cond_init,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_init hijacked" << std::endl);
    }

    // Look for pthread_cond_signal and hijack it
    rtn = RTN_FindByName(img, "pthread_cond_signal");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_signal on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_cond_signal,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_signal hijacked" << std::endl);
    }

    // Look for pthread_cond_wait and hijack it
    rtn = RTN_FindByName(img, "pthread_cond_wait");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_wait on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_cond_wait,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_wait hijacked" << std::endl);
    }

    // Look for pthread_create and insert callbacks
    rtn = RTN_FindByName(img, "pthread_create");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_create on image" << std::endl);
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)before_create,
                       IARG_FUNCARG_CALLSITE_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)after_create,
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);
        DEBUG(cerr << "pthread_create registered" << std::endl);
    }

    // Look for pthread_join and hijack it
    rtn = RTN_FindByName(img, "pthread_join");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_join on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hj_pthread_join,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_join hijacked" << std::endl);
    }
}

VOID thread_start_callback(THREADID thread_id, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    cerr << "[PINocchio] Thread Initialized: " << thread_id << std::endl;

    // Check if thread id is under limit
    if(thread_id >= MAX_THREADS) {
        cerr << "[PINocchio] Internal error: Too many threads, can't create thread " << thread_id << std::endl;
        fail();
    }

    // Create register action
    ACTION action = {
        thread_id,
        ACTION_REGISTER,
    };
    sync(&action);
}

VOID thread_fini_callback(THREADID thread_id, CONTEXT const *ctxt, INT32 flags, VOID *v)
{
    cerr << "[PINocchio] Thread Finished: " << thread_id << std::endl;

    ACTION action = {
        thread_id,
        ACTION_FINI,
    };
    sync(&action);
}

VOID Fini(INT32 code, VOID *v)
{
    trace_bank_dump();
    trace_bank_free();
    cerr << "===============================================" << std::endl;
    cerr << " PINocchio exiting " << std::endl;
    cerr << "===============================================" << std::endl;
}

VOID ins_handler(THREADID tid)
{
    // Instruction callback, ONLY update instruction counter
    all_threads[(int)tid].ins_count++;
}

VOID mem_ins_handler(THREADID tid)
{
    // Memory Instruction callback, update instruction counter
    all_threads[(int)tid].ins_count++;

    // Sync, which could make it sleep
    ACTION action = {
        .tid = tid,
        .action_type = ACTION_DONE,
    };
    sync(&action);

}

VOID instruction(INS ins, VOID *v)
{
    if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_ins_handler,
                        IARG_THREAD_ID, IARG_END);
    } else {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_handler,
                        IARG_THREAD_ID, IARG_END);
    }
}

int main(int argc, char *argv[])
{
    knob_welcome();

    // Print usage if help was called.
    if(PIN_Init(argc, argv)) {
        return knob_usage();
    }

    // Initialize sync structure
    sync_init();

    // Hadler for instructions
    INS_AddInstrumentFunction(instruction, 0);

    // Handler for thread creation
    PIN_AddThreadStartFunction(thread_start_callback, 0);

    // Handler for thread Fini
    PIN_AddThreadFiniFunction(thread_fini_callback, 0);

    // Handler for mutex functions
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(module_load_handler, NULL);

    // Handler for exit
    PIN_AddFiniFunction(Fini, 0);

    // PIN_StartProgram() is not expected to return
    PIN_StartProgram();
    return 0;
}
