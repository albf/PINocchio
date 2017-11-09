#ifndef __LOG_H__
#define __LOG_H__

#define MAXLOGSIZE 2048           // Must be power of REDUCTIONSTEP
#define REDUCTIONSTEP 8           // TODO Fix by adding remainings on new buffer
#define OUTPUTFILE "trace.json"   // TODO: Change how name is defined

#include "sync.h"

typedef struct {
    // Buffer related fields
    int buffer[POSSIBLE_STATES][MAX_THREADS];
    int buffer_size;
    int buffer_capacity;

    // Log related fields
    THREAD_STATUS log[MAXLOGSIZE][MAX_THREADS];
    int log_start[MAX_THREADS];
    int log_next;
} T_log;

extern T_log *tlog;

// Init the log, allocating memory and initializing required fields.
void log_init();

// Flush status information into log structure.
void log_add();

// Dump current log to external file.
void log_dump();

// Free flusher allocated memory.
void log_free();

#endif
