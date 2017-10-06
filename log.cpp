#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

T_log *tlog;

// Internal auxiliary functions
int log_on_buffer();
int log_merge(THREAD_STATUS v[MAXLOGSIZE][MAX_THREADS], int *next, int sizeofv, int rs);
int log_make_status_string(char *s, int index);

void log_init()
{
    int i, j;
    cerr << "[LOG] Log Starting..." << std::endl;
    tlog = (T_log *) malloc(sizeof(T_log));
    if(tlog == NULL) {
        cerr << "[LOG] ERROR: Allocation error. Exiting" << std::endl;
        exit(-1);
    }

    // Init log fields
    tlog->log_next = 0;
    for(i = 0; i < MAX_THREADS; i++) {
        tlog->log_start[i] = -1;
    }

    // Init buffer fields
    tlog->buffer_next = 0;
    tlog->buffer_size = 1;

    for(j = 0; j < MAX_THREADS; j++) {
        for(i = 0; i < MAXLOGSIZE; i++) {
            tlog->buffer[i][j] = UNREGISTERED;
            tlog->log[i][j] = UNREGISTERED;
        }
    }
    cerr << "[LOG] Log Initiated" << std::endl;
}

// log_on_buffer returns 1 if it became full, 0 otherwise.
int log_on_buffer()
{
    unsigned int i;

    // Log all threads status
    for(i = 0; i <= max_tid; i++) {
        // If just start, mark its new position
        if(tlog->log_start[i] < 0) {
            tlog->log_start[i] = tlog->log_next;
        }

        tlog->buffer[tlog->buffer_next][i] = all_threads[i].status;
    }

    tlog->buffer_next++;

    if(tlog->buffer_next == tlog->buffer_size) {
        return 1;
    }

    return 0;
}

void log_add()
{
    int is_buffer_full;
    unsigned int i;

    if(tlog->buffer_size == 1) {
        // First case, ignore buffer since it would be flushed anyway.
        for(i = 0; i <= max_tid; i++) {
            if(tlog->log_start[i] < 0) {
                tlog->log_start[i] = tlog->log_next;
            }
            tlog->log[tlog->log_next][i] = all_threads[i].status;
        }
        tlog->buffer_next = 0;
    } else {
        // First insert on buffer, checking if full
        is_buffer_full = log_on_buffer();
        if(is_buffer_full == 0) {
            return;
        }

        // If full, merge and continue updating log
        log_merge(tlog->buffer, tlog->log_start,
                  tlog->buffer_size, tlog->buffer_size);
        for(i = 0; i <= max_tid; i++) {
            tlog->log[tlog->log_next][i] = tlog->buffer[0][i];
        }
        tlog->buffer_next = 0;
    }

    // Check if should merge log too.
    tlog->log_next++;
    if(tlog->log_next == MAXLOGSIZE) {
        tlog->log_next = log_merge(tlog->log, tlog->log_start, MAXLOGSIZE, REDUCTIONSTEP);

        // Update buffer size, since it was merged and should be increased.
        tlog->buffer_size *= REDUCTIONSTEP;
        if(tlog->buffer_size > MAXLOGSIZE) {
            puts("Internal Error: Can't increase buffer size anymore.");
            tlog->buffer_size = MAXLOGSIZE;
        }
    }
}

// Returns the new next position available.
int log_merge(THREAD_STATUS v[MAXLOGSIZE][MAX_THREADS], int *next, int sizeofv, int rs)
{
    int i, j, max;
    unsigned int k;
    // i=5 ; REDUCTIONSTEP = 5

    // next |... 5 6 7 8 9 ... SUM
    // 0    |... 1 1 1 0 0 ... 3 3 -> 1
    // 3    |... 1 1 0 1 1 ... 4 4 -> 1
    // 7    |... 1 1 1 1 0 ... 4 2 -> 0
    // 12   |... 1 1 1 1 1 ... 5 0 -> 1

    for(i = 0; i < sizeofv; i += rs) { // Iterate over blocks to shrink
        // Last vector @ each reduction block is taken as reference
        // (includes all threads)

        for(k = 0; k < max_tid ; k++) {  // Iterate over each threadlog  (\/)
            int possible_states[4] = {0, 0, 0, 0};
            for(j = i; j < (i + rs); j++) { // Iterate inside a shrinking block (->)
                possible_states[(int)v[j][k]]++;
            }
            // Use the most frequent state on block.
            max = 0;
            //cerr << "Merging[" << k << "][" << j << "] = " << possible_states[0] << std::endl;
            for(j = 1; j < POSSIBLE_STATES; j++) {
                //cerr << "Merging[" << k << "][" << j << "] = " << possible_states[j] << std::endl;
                if(possible_states[j] > possible_states[max]) {
                    max = j;
                }
            }
            v[i][k] = (THREAD_STATUS) max;
            //cerr << "Merged[" << k << "] into " << max << std::endl;
        }
    }

    return sizeofv / rs;
}

void log_free()
{
    cerr << "[LOG] Cleaning memory space" << std::endl;
    free(tlog);
}

void log_dump()
{
    char str[MAXLOGSIZE + 1];
    ofstream f;

    cerr << "[LOG] Dumping report to trace.json" << std::endl;

    f.open(OUTPUTFILE);

    f << "{\n" <<
      "  \"end\":" << tlog->log_next << ",\n" <<
      "  \"sample-size\":" << tlog->buffer_size << ",\n";

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
int log_make_status_string(char *s, int index)
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
