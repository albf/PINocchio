#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

T_log *tlog;

// Internal auxiliary functions
int log_on_buffer();
int log_merge(THREAD_STATUS v[MAXLOGSIZE][MAX_THREADS], int *next, int sizeofv, int rs);
char *log_make_status_string(char *s, int index);

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
            tlog->buffer[i][j] = UNLOCKED;
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
            tlog->log_start[i] = tlog->log_next; // TESTING
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
    int i, j, k;
    // i=5 ; REDUCTIONSTEP = 5

    // next |... 5 6 7 8 9 ... SUM
    // 0    |... 1 1 1 0 0 ... 3 3 -> 1
    // 3    |... 1 1 0 1 1 ... 4 4 -> 1
    // 7    |... 1 1 1 1 0 ... 4 2 -> 0
    // 12   |... 1 1 1 1 1 ... 5 0 -> 1

    for(i = 0; i < sizeofv; i += rs) { // Iterate over blocks to shrink
        // Last vector @ each reduction block is taken as reference
        // (includes all threads)

        for(k = 0; k < MAX_THREADS; k++) {  // Iterate over each threadlog  (\/)
            int possible_states[2] = {0, 0};
            for(j = i; j < (i + rs - 1); j++) { // Iterate inside a shrinking block (->)
                if(v[j][k] == UNLOCKED) {
                    possible_states[0]++;
                } else {
                    possible_states[1]++;
                }
            }
            // Use the most frequent state on block.
            if(possible_states[0] > possible_states[1]) {
                v[i][k] = UNLOCKED;
            } else {
                v[i][k] = LOCKED;
            }
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

    f << "{\n\t\"END\":" << tlog->log_next <<
      ",\n\t\"MAIN\":{\n\t\t\"STP\":" << tlog->log_start[0] <<
      ",\n\t\t\"STR\":\"" << log_make_status_string(str, 0) <<
      "\"\n\t},\n";

    f << "\t\"CTRL\":{\n\t\t\"STP\":" << tlog->log_start[1] <<
      ",\n\t\t\"STR\":\"" << log_make_status_string(str, 1) <<
      "\"\n\t},\n";

    f << "\t\"THREADS\": [\n";

    int i;
    for(i = 2; i < MAX_THREADS; i++) {
        f << "\t\t{\n \t\t\t\"STP\":" << tlog->log_start[i] <<
          ",\n\t\t\t\"STR\":\"" << log_make_status_string(str, i) <<
          "\"\n\t\t}" << (i == (MAX_THREADS - 1) ? "" : ",") <<
          "\n";
    }

    f << "\t]\n}" << std::endl;
    //f.flush();
    f.close();
}

char *log_make_status_string(char *s, int index)
{
    int i;
    int start = tlog->log_start[index];

    if(start == -1) {
        s[0] = '\0';
        return s;
    }

    for(i = start; i < tlog->log_next; i++) {
        s[i - start] = (char)(0x30 + tlog->log[i][index]); // Inefficient...
    }
    s[tlog->log_next - start] = '\0';
    return s;
}
