#ifndef __TRACE_H__
#define __TRACE_H__

#define MAX_BANK_SIZE 4096          // Max number of changes per threads.
                                    // Once it's reached, some smaller ones will be lost.

#define REDUCTION_SIZE 128          // Number of traces to be removed once limit is reached.

#include "thread.h"
#include "pin.H"

typedef struct {
    UINT64 time;
    THREAD_STATUS status;
} CHANGE;

typedef struct {
    UINT64 start;
    UINT64 end;

    int total_changes;
    CHANGE *changes;
} P_TRACE;

// Init trace bank, allocating memory and initializing required fields.
void trace_bank_init(int pram);

// Register a newly created thread. Will consider it active during start.
void trace_bank_register(THREADID tid, UINT64 time);

// Insert the change on the status on the trace array.
void trace_bank_update(THREADID tid, UINT64 time, THREAD_STATUS status);

// Insert the change on the status on the trace array.
void trace_bank_finish(THREADID tid, UINT64 time);

// Dump current trace bank  to external file.
void trace_bank_dump();

// Free flusher allocated memory.
void trace_bank_free();

// Debug function to print trace_bank current state.
void trace_bank_print();

// Debug function to validate if the trace is healthy.
void trace_bank_validate();

#endif
