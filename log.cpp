#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

T_log *tlog;

// Internal auxiliary functions
int log_on_buffer();
void reset_buffer();
void force_buffer_release();
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

    // Init buffer fields
    tlog->buffer_capacity = 1;
    reset_buffer();


    // Init log fields
    tlog->log_next = 0;
    for(i = 0; i < MAX_THREADS; i++) {
        tlog->log_start[i] = -1;
    }

    for(j = 0; j < MAX_THREADS; j++) {
        for(i = 0; i < MAXLOGSIZE; i++) {
            tlog->log[i][j] = UNREGISTERED;
        }
    }

    cerr << "[LOG] Log Initiated" << std::endl;
}

// log_on_buffer returns 1 if it became full, 0 otherwise.
int log_on_buffer()
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

void reset_buffer()
{
    for(unsigned int i = 0; i < max_tid; i++) {
        for(int j = 0; j < POSSIBLE_STATES; j++) {
            tlog->buffer[j][i] = 0;
        }
    }
    tlog->buffer_size = 0;
}

void force_buffer_release()
{
    for(int i = tlog->buffer_size; i < tlog->buffer_capacity; i++) {
        log_on_buffer();
    }
}

void log_add()
{
    int is_buffer_full;
    unsigned int i, j;

    if(tlog->buffer_capacity == 1) {
        // First case, ignore buffer since it would be flushed anyway.
        for(i = 0; i <= max_tid; i++) {
            if(tlog->log_start[i] < 0) {
                tlog->log_start[i] = tlog->log_next;
            }
            tlog->log[tlog->log_next][i] = all_threads[i].status;
        }
    } else {
        // First insert on buffer, checking if full
        is_buffer_full = log_on_buffer();
        if(is_buffer_full == 0) {
            return;
        }

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
    }

    // Check if should merge log too.
    tlog->log_next++;
    if(tlog->log_next == MAXLOGSIZE) {
        tlog->log_next = log_merge(tlog->log, tlog->log_start, MAXLOGSIZE, REDUCTIONSTEP);

        // Update buffer size, since it was merged and should be increased.
        tlog->buffer_capacity *= REDUCTIONSTEP;
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
