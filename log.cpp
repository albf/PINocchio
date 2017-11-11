#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

T_log *tlog;

// Internal auxiliary functions
static int log_on_buffer();
static void reset_buffer();
static void force_buffer_release();
static int log_merge();
static int log_make_status_string(char *s, int index);

void log_init()
{
    int i, j;
    cerr << "[LOG] Log Starting..." << std::endl;
    tlog = (T_log *) malloc(sizeof(T_log));
    if(tlog == NULL) {
        cerr << "[LOG] ERROR: Allocation error. Exiting" << std::endl;
        exit(-1);
    }

    // Init buffer fields
    tlog->buffer_capacity = 1;
    reset_buffer();


    // Init log fields
    tlog->log_next = 0;
    for(i = 0; i < MAX_THREADS; i++) {
        tlog->log_start[i] = -1;
    }

    for(j = 0; j < MAX_THREADS; j++) {
        for(i = 0; i < MAX_LOG_SIZE; i++) {
            tlog->log[i][j] = UNREGISTERED;
        }
    }

    cerr << "[LOG] Log Initiated" << std::endl;
}

// log_on_buffer returns 1 if it became full, 0 otherwise.
static int log_on_buffer()
{
    // Log all threads status
    for(unsigned int i = 0; i <= max_tid; i++) {
        // If just start, mark its new position
        if(tlog->log_start[i] < 0) {
            tlog->log_start[i] = tlog->log_next;
        }

        tlog->buffer[(int)all_threads[i].status][i]++;
    }

    tlog->buffer_size++;
    if(tlog->buffer_size == tlog->buffer_capacity) {
        return 1;
    }

    return 0;
}

static void reset_buffer()
{
    for(unsigned int i = 0; i <= max_tid; i++) {
        for(int j = 0; j < POSSIBLE_STATES; j++) {
            tlog->buffer[j][i] = 0;
        }
    }
    tlog->buffer_size = 0;
}

static void force_buffer_release()
{
    for(int i = tlog->buffer_size; i < tlog->buffer_capacity; i++) {
        log_on_buffer();
    }
}

void log_add()
{
    unsigned int i, j;

    // First insert on buffer, checking if full. If not, just return.
    if(log_on_buffer() == 0) {
        return;
    }

    // Buffer is full, add a new position on the log.
    for(i = 0; i <= max_tid; i++) {
        int max = 0;
        for(j = 1; j < POSSIBLE_STATES; j++) {
            if(tlog->buffer[j][i] > tlog->buffer[max][i]) {
                max = j;
            }
        }
        tlog->log[tlog->log_next][i] = (THREAD_STATUS)max;
    }
    reset_buffer();

    // Update position and check if should also merge. 
    tlog->log_next++;
    if(tlog->log_next == MAX_LOG_SIZE) {
        tlog->log_next = log_merge();

        // Update buffer size, since it was merged and should be increased.
        tlog->buffer_capacity *= REDUCTION_STEP;
    }
}

// Returns the new next position available.
static int log_merge()
{
    int i, j, max;
    unsigned int k;

    // STATE/THREAD |... 0 1 2
    // ---------------------------
    // 0            |... 0 2 2
    // 1            |... 0 5 2
    // 2            |... 8 1 2
    // 3            |... 0 0 2
    // ---------------------------
    // Result State |... 2 1 0

    for(k = 0; k <= max_tid ; k++) {                         // Iterate over each threadlog

        if (tlog->log_start[k] < 0) {                        // Ignore non started threads
            continue;
        }

        // update log_start, for each thread
        tlog->log_start[k] = tlog->log_start[k]/REDUCTION_STEP;

        for(i = 0; i < MAX_LOG_SIZE; i += REDUCTION_STEP) {  // Iterate over blocks to shrink
            int possible_states[4] = {0, 0, 0, 0};

            for(j = i; j < (i + REDUCTION_STEP); j++) {      // Iterate inside a shrinking block
                possible_states[(int)tlog->log[j][k]]++;
            }

            // Use the most frequent state on block.
            max = 0;
            for(j = 1; j < POSSIBLE_STATES; j++) {
                if(possible_states[j] > possible_states[max]) {
                    max = j;
                }
            }

            // Beware: the new position for the block is i/REDUCTION_STEP
            tlog->log[i/REDUCTION_STEP][k] = (THREAD_STATUS) max;
        }
    }

    return MAX_LOG_SIZE / REDUCTION_STEP;
}

void log_free()
{
    cerr << "[LOG] Cleaning memory space" << std::endl;
    free(tlog);
}

void log_dump()
{
    char str[MAX_LOG_SIZE + 1];
    ofstream f;

    cerr << "[LOG] Dumping report to trace.json" << std::endl;

    force_buffer_release();
    f.open(OUTPUTFILE);

    f << "{\n" <<
      "  \"end\":" << tlog->log_next << ",\n" <<
      "  \"sample-size\":" << tlog->buffer_capacity << ",\n";

    f << "  \"threads\": [\n";
    int first = 1;
    for(int i = 0; i < MAX_THREADS; i++) {
        int exist = log_make_status_string(str, i);

        if(exist > 0) {
            if(first > 0) {
                first = 0;
            } else {
                f << ",\n";
            }

            f << "    {\n" <<
              "      \"pin-tid\":" << i << ",\n" <<
              "      \"start\":" << tlog->log_start[i] << ",\n" <<
              "      \"samples\":\"" << str << "\"\n" <<
              "    }";
        }
    }

    f << "\n  ]\n}\n";
    f.close();
}

// Returns 0 if stayed unregistered the whole time
static int log_make_status_string(char *s, int index)
{
    int any = 0;
    int start = tlog->log_start[index];

    if(start < 0) {
        return 0;
    }

    for(int i = start; i < tlog->log_next; i++) {
        s[i - start] = (char)(0x30 + tlog->log[i][index]);
        if(tlog->log[i][index] != UNREGISTERED) {
            any = 1;
        }
    }

    s[tlog->log_next - start] = '\0';
    return any;
}
