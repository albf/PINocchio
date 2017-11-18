#ifndef __TRACE_H__
#define __TRACE_H__

#define MAX_BANK_SIZE 2048           // Must be power of REDUCTIONSTEP
#define REDUCTION_STEP 8            // TODO Fix by adding remainings on new buffer
#define OUTPUTFILE "trace.json"     // TODO: Change how name is defined

#include "sync.h"

typedef struct {
    // Buffer related fields
    int buffer[POSSIBLE_STATES][MAX_THREADS];
    int buffer_size;
    int buffer_capacity;

    // Bank related fields
    THREAD_STATUS bank[MAX_BANK_SIZE][MAX_THREADS];
    int ins_start[MAX_THREADS];
    int ins_next;
} T_bank;

extern T_bank *t_bank;

// Init trace bank, allocating memory and initializing required fields.
void trace_bank_init();

// Flush status information into trace bank structure.
void trace_bank_add();

// Dump current trace bank  to external file.
void trace_bank_dump();

// Free flusher allocated memory.
void trace_bank_free();

#endif
