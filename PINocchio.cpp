// Sync related
#include "sync.h"
#include "trace_bank.h"

// Pin related
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include "log.h"
#include "knob.h"
#include "pin.H"

static int sync_period;

/*
pthread_mutex_lock, pthread_mutex_trylock, pthread_mutex_unlock
sem_destroy, sem_getvalue, sem_init, sem_post, sem_trywait and sem_wait
will have their original calls replaced. Create and join follow different
rules, create need both before and after callbacks.
*/

/* Mutex hooks */

int hk_pthread_mutex_destroy(pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(cerr << "mutex_destroy called: " << mutex << std::endl);

    ACTION action = {
        tid,
        ACTION_LOCK_DESTROY,
        {(void *) mutex},
    };
    sync(&action);

    return 0;
}

int hk_pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t attr, THREADID tid)
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

int hk_pthread_mutex_lock(pthread_mutex_t *mutex, THREADID tid)
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

int hk_pthread_mutex_trylock(pthread_mutex_t *mutex, THREADID tid)
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

int hk_pthread_mutex_unlock(pthread_mutex_t *mutex, THREADID tid)
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

/* Semaphore hooks */

int hk_sem_destroy(sem_t *sem, THREADID tid)
{
    DEBUG(cerr << "sem_destroy called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_DESTROY,
        {(void *) sem},
    };
    sync(&action);

    return 0;
}

int hk_sem_getvalue(sem_t *sem, int *value, THREADID tid)
{
    DEBUG(cerr << "sem_getvalue called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_GETVALUE,
        {(void *) sem},
    };
    sync(&action);

    return action.arg.i;
}

int hk_sem_init(sem_t *sem, int pshared, unsigned int value, THREADID tid)
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

int hk_sem_post(sem_t *sem, THREADID tid)
{
    DEBUG(cerr << "sem_post called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_POST,
        {(void *) sem},
    };
    sync(&action);

    return 0;
}

int hk_sem_trywait(sem_t *sem, THREADID tid)
{
    DEBUG(cerr << "sem_trywait called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_TRYWAIT,
        {(void *) sem},
    };
    sync(&action);

    return action.arg.i;
}

int hk_sem_wait(sem_t *sem, THREADID tid)
{
    DEBUG(cerr << "sem_wait called: " << sem << std::endl);

    ACTION action = {
        tid,
        ACTION_SEM_WAIT,
        {(void *) sem},
    };
    sync(&action);

    return 0;
}



/* Rwlock hooks */

int hk_pthread_rwlock_destroy(pthread_rwlock_t *rwlock, THREADID tid)
{
    DEBUG(cerr << "pthread_rwlock_destroy called." << std::endl);

    ACTION action = {
        tid,
        ACTION_RWLOCK_DESTROY,
        {(void *) rwlock},
    };
    sync(&action);

    return 0;
}

int hk_pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr, THREADID tid)
{
    DEBUG(cerr << "pthread_rwlock_init called." << std::endl);

    ACTION action = {
        tid,
        ACTION_RWLOCK_INIT,
        {(void *) rwlock},
    };
    sync(&action);

    return 0;
}

int hk_pthread_rwlock_rdlock(pthread_rwlock_t *rwlock, THREADID tid)
{
    DEBUG(cerr << "pthread_rwlock_rdlock called." << std::endl);

    ACTION action = {
        tid,
        ACTION_RWLOCK_RDLOCK,
        {(void *) rwlock},
    };
    sync(&action);

    return 0;
}

int hk_pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock, THREADID tid)
{
    DEBUG(cerr << "pthread_rwlock_tryrdlock called." << std::endl);

    ACTION action = {
        tid,
        ACTION_RWLOCK_TRYRDLOCK,
        {(void *) rwlock},
    };
    sync(&action);

    return action.arg.i;
}

int hk_pthread_rwlock_wrlock(pthread_rwlock_t *rwlock, THREADID tid)
{
    DEBUG(cerr << "pthread_rwlock_wrlock called." << std::endl);

    ACTION action = {
        tid,
        ACTION_RWLOCK_WRLOCK,
        {(void *) rwlock},
    };
    sync(&action);

    return 0;
}

int hk_pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock, THREADID tid)
{
    DEBUG(cerr << "pthread_rwlock_trywrlock called." << std::endl);

    ACTION action = {
        tid,
        ACTION_RWLOCK_TRYWRLOCK,
        {(void *) rwlock},
    };
    sync(&action);

    return action.arg.i;
}

int hk_pthread_rwlock_unlock(pthread_rwlock_t *rwlock, THREADID tid)
{
    DEBUG(cerr << "pthread_rwlock_unlock called." << std::endl);

    ACTION action = {
        tid,
        ACTION_RWLOCK_UNLOCK,
        {(void *) rwlock},
    };
    sync(&action);

    return 0;
}



/* Conditional hooks  */

int hk_pthread_cond_broadcast(pthread_cond_t *cond, THREADID tid)
{
    DEBUG(cerr << "pthread_cond_broadcast called: " << cond << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_BROADCAST,
        {(void *) cond},
    };
    sync(&action);

    return 0;
}

int hk_pthread_cond_destroy(pthread_cond_t *cond, THREADID tid)
{
    DEBUG(cerr << "pthread_cond_destroy called: " << cond << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_DESTROY,
        {(void *) cond},
    };
    sync(&action);

    return 0;
}

int hk_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr, THREADID tid)
{
    DEBUG(cerr << "pthread_cond_init called: " << cond << std::endl);

    if(attr != NULL) {
        cerr << "Error: pthread_cond_init attr should be NULL on tid: " << print_id(tid) << std::endl;
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

int hk_pthread_cond_signal(pthread_cond_t *cond, THREADID tid)
{
    DEBUG(cerr << "pthread_cond_signal called: " << cond << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_SIGNAL,
        {(void *) cond},
    };
    sync(&action);

    return 0;
}

int hk_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, THREADID tid)
{
    DEBUG(cerr << "pthread_cond_wait called: " << cond << ", " << mutex << std::endl);

    ACTION action = {
        tid,
        ACTION_COND_WAIT,
        {(void *) cond, (void *) mutex},
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

int hk_pthread_join(pthread_t thread, THREADID tid)
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

    // Look for pthread_mutex_init and replace by hook
    rtn = RTN_FindByName(img, "pthread_mutex_init");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_init on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_mutex_init,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_init replaced" << std::endl);
    }

    // Look for pthread_mutex_destroy and replace by hook
    rtn = RTN_FindByName(img, "pthread_mutex_destroy");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_destroy on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_mutex_destroy,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_destroy replaced" << std::endl);
    }

    // Look for pthread_mutex_lock and replace by hook
    rtn = RTN_FindByName(img, "pthread_mutex_lock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_lock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_mutex_lock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_lock replaced" << std::endl);
    }

    // Look for pthread_mutex_trylock and replace by hook
    rtn = RTN_FindByName(img, "pthread_mutex_trylock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_trylock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_mutex_trylock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_trylock replaced" << std::endl);
    }

    // Look for pthread_mutex_unlock and replace by hook
    rtn = RTN_FindByName(img, "pthread_mutex_unlock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_mutex_unlock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_mutex_unlock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_mutex_unlock replaced" << std::endl);
    }

    // Look for sem_destroy and replace by hook
    rtn = RTN_FindByName(img, "sem_destroy");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_destroy on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_sem_destroy,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_destroy replaced" << std::endl);
    }

    // Look for sem_getvalue and replace by hook
    rtn = RTN_FindByName(img, "sem_getvalue");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_getvalue on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_sem_getvalue,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_getvalue replaced" << std::endl);
    }

    // Look for sem_init and replace by hook
    rtn = RTN_FindByName(img, "sem_init");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_init on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_sem_init,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_init replaced" << std::endl);
    }

    // Look for sem_post and replace by hook
    rtn = RTN_FindByName(img, "sem_post");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_post on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_sem_post,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_post replaced" << std::endl);
    }

    // Look for sem_trywait and replace by hook
    rtn = RTN_FindByName(img, "sem_trywait");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_trywait on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_sem_trywait,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_trywait replaced" << std::endl);
    }

    // Look for sem_wait and replace by hook
    rtn = RTN_FindByName(img, "sem_wait");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found sem_wait on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_sem_wait,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "sem_wait replaced" << std::endl);
    }

    // Look for pthread_rwlock_destroy and replace by hook
    rtn = RTN_FindByName(img, "pthread_rwlock_destroy");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_rwlock_destroy on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_rwlock_destroy,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_rwlock_destroy replaced" << std::endl);
    }

    // Look for pthread_rwlock_init and replace by hook
    rtn = RTN_FindByName(img, "pthread_rwlock_init");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_rwlock_init on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_rwlock_init,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_rwlock_init replaced" << std::endl);
    }

    // Look for pthread_rwlock_rdlock and replace by hook
    rtn = RTN_FindByName(img, "pthread_rwlock_rdlock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_rwlock_rdlock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_rwlock_rdlock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_rwlock_rdlock replaced" << std::endl);
    }

    // Look for pthread_rwlock_tryrdlock and replace by hook
    rtn = RTN_FindByName(img, "pthread_rwlock_tryrdlock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_rwlock_tryrdlock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_rwlock_tryrdlock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_rwlock_tryrdlock replaced" << std::endl);
    }

    // Look for pthread_rwlock_wrlock and replace by hook
    rtn = RTN_FindByName(img, "pthread_rwlock_wrlock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_rwlock_wrlock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_rwlock_wrlock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_rwlock_wrlock replaced" << std::endl);
    }

    // Look for pthread_rwlock_trywrlock and replace by hook
    rtn = RTN_FindByName(img, "pthread_rwlock_trywrlock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_rwlock_trywrlock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_rwlock_trywrlock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_rwlock_trywrlock replaced" << std::endl);
    }

    // Look for pthread_rwlock_unlock and replace by hook
    rtn = RTN_FindByName(img, "pthread_rwlock_unlock");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_rwlock_unlock on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_rwlock_unlock,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_rwlock_unlock replaced" << std::endl);
    }

    // Look for pthread_cond_broadcast and replace by hook
    rtn = RTN_FindByName(img, "pthread_cond_broadcast");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_broadcast on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_cond_broadcast,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_broadcast replaced" << std::endl);
    }

    // Look for pthread_cond_destroy and replace by hook
    rtn = RTN_FindByName(img, "pthread_cond_destroy");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_destroy on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_cond_destroy,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_destroy replaced" << std::endl);
    }

    // Look for pthread_cond_init and replace by hook
    rtn = RTN_FindByName(img, "pthread_cond_init");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_init on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_cond_init,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_init replaced" << std::endl);
    }

    // Look for pthread_cond_signal and replace by hook
    rtn = RTN_FindByName(img, "pthread_cond_signal");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_signal on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_cond_signal,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_signal replaced" << std::endl);
    }

    // Look for pthread_cond_wait and replace by hook
    rtn = RTN_FindByName(img, "pthread_cond_wait");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_cond_wait on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_cond_wait,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_cond_wait replaced" << std::endl);
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

    // Look for pthread_join and replace by hook
    rtn = RTN_FindByName(img, "pthread_join");
    if(RTN_Valid(rtn)) {
        DEBUG(cerr << "Found pthread_join on image" << std::endl);
        RTN_ReplaceSignature(rtn, (AFUNPTR)hk_pthread_join,
                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                             IARG_THREAD_ID, IARG_END);
        DEBUG(cerr << "pthread_join replaced" << std::endl);
    }
}

VOID thread_start_callback(THREADID thread_id, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    cerr << "[PINocchio] Thread Initialized: " << print_id(thread_id) << std::endl;

    // Check if thread id is under limit
    if(thread_id >= MAX_THREADS) {
        cerr << "[PINocchio] Internal error: Too many threads, can't create thread " << print_id(thread_id) << std::endl;
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
    cerr << "[PINocchio] Thread Finished: " << print_id(thread_id) << std::endl;

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
    if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_ins_handler,
                       IARG_THREAD_ID, IARG_END);
    } else {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ins_handler,
                       IARG_THREAD_ID, IARG_END);
    }
}



// Slightly different version when period is not 0 and results are approximate.

VOID mem_ins_handler_approximate(THREADID tid)
{
    // Memory Instruction callback, update instruction counter
    all_threads[(int)tid].ins_count++;

    if(((all_threads[(int)tid].ins_count - all_threads[(int)tid].sync_holder) / sync_period) > 0) {
        all_threads[(int)tid].sync_holder = all_threads[(int)tid].ins_count;

        // For approximate measure, some sync will be ignored.
        ACTION action = {
            .tid = tid,
            .action_type = ACTION_DONE,
        };
        sync(&action);
    }
}

VOID instruction_approximate(INS ins, VOID *v)
{
    if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)mem_ins_handler_approximate,
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

    bool pram = !knob_time_based.Value();
    sync_period = knob_sync_frenquency.Value();

    // Initialize sync structure
    sync_init(pram);

    // Hadler for instructions
    if(pram > 0) {
        if(sync_period == 1) {
            INS_AddInstrumentFunction(instruction, 0);
        } else {
            INS_AddInstrumentFunction(instruction_approximate, 0);
        }
    }

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
