#include "trace_bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

T_bank *t_bank;

// Internal auxiliary functions
static int trace_bank_record_on_buffer();
static void reset_buffer();
static void force_buffer_release();
static int merge_buffer();
static int make_status_string(char *s, int index);

void trace_bank_init()
{
    int i, j;
    cerr << "[Trace Bank] Bank Initializing..." << std::endl;
    t_bank = (T_bank *) malloc(sizeof(T_bank));
    if(t_bank == NULL) {
        cerr << "[Trace Bank] ERROR: Allocation error. Exiting" << std::endl;
        exit(-1);
    }

    // Init buffer fields
    t_bank->buffer_capacity = 1;
    reset_buffer();


    // Init bank fields
    t_bank->ins_next = 0;
    for(i = 0; i < MAX_THREADS; i++) {
        t_bank->ins_start[i] = -1;
    }

    for(j = 0; j < MAX_THREADS; j++) {
        for(i = 0; i < MAX_BANK_SIZE; i++) {
            t_bank->bank[i][j] = UNREGISTERED;
        }
    }

    cerr << "[Trace Bank] Bank Initiated" << std::endl;
}

// trace_bank_record_on_buffer returns 1 if it became full, 0 otherwise.
static int trace_bank_record_on_buffer()
{
    // Record all threads status
    for(unsigned int i = 0; i <= max_tid; i++) {
        // If just start, mark its new position
        if(t_bank->ins_start[i] < 0) {
            t_bank->ins_start[i] = t_bank->ins_next;
        }

        t_bank->buffer[(int)all_threads[i].status][i]++;
    }

    t_bank->buffer_size++;
    if(t_bank->buffer_size == t_bank->buffer_capacity) {
        return 1;
    }

    return 0;
}

static void reset_buffer()
{
    for(unsigned int i = 0; i <= max_tid; i++) {
        for(int j = 0; j < POSSIBLE_STATES; j++) {
            t_bank->buffer[j][i] = 0;
        }
    }
    t_bank->buffer_size = 0;
}

static void force_buffer_release()
{
    for(int i = t_bank->buffer_size; i < t_bank->buffer_capacity; i++) {
        trace_bank_record_on_buffer();
    }
}

void trace_bank_add()
{
    unsigned int i, j;

    // First insert on buffer, checking if full. If not, just return.
    if(trace_bank_record_on_buffer() == 0) {
        return;
    }

    // Buffer is full, add a new position on the bank.
    for(i = 0; i <= max_tid; i++) {
        int max = 0;
        for(j = 1; j < POSSIBLE_STATES; j++) {
            if(t_bank->buffer[j][i] > t_bank->buffer[max][i]) {
                max = j;
            }
        }
        t_bank->bank[t_bank->ins_next][i] = (THREAD_STATUS)max;
    }
    reset_buffer();

    // Update position and check if should also merge. 
    t_bank->ins_next++;
    if(t_bank->ins_next == MAX_BANK_SIZE) {
        t_bank->ins_next = merge_buffer();

        // Update buffer size, since it was merged and should be increased.
        t_bank->buffer_capacity *= REDUCTION_STEP;
    }
}

// Returns the new next position available.
static int merge_buffer()
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

    for(k = 0; k <= max_tid ; k++) {                         // Iterate over each threadbank

        if (t_bank->ins_start[k] < 0) {                        // Ignore non started threads
            continue;
        }

        // update ins_start, for each thread
        t_bank->ins_start[k] = t_bank->ins_start[k]/REDUCTION_STEP;

        for(i = 0; i < MAX_BANK_SIZE; i += REDUCTION_STEP) {  // Iterate over blocks to shrink
            int possible_states[4] = {0, 0, 0, 0};

            for(j = i; j < (i + REDUCTION_STEP); j++) {      // Iterate inside a shrinking block
                possible_states[(int)t_bank->bank[j][k]]++;
            }

            // Use the most frequent state on block.
            max = 0;
            for(j = 1; j < POSSIBLE_STATES; j++) {
                if(possible_states[j] > possible_states[max]) {
                    max = j;
                }
            }

            // Beware: the new position for the block is i/REDUCTION_STEP
            t_bank->bank[i/REDUCTION_STEP][k] = (THREAD_STATUS) max;
        }
    }

    return MAX_BANK_SIZE / REDUCTION_STEP;
}

void trace_bank_free()
{
    cerr << "[Trace Bank] Cleaning memory space" << std::endl;
    free(t_bank);
}

void trace_bank_dump()
{
    char str[MAX_BANK_SIZE + 1];
    ofstream f;

    cerr << "[Trace Bank] Dumping report to trace.json" << std::endl;

    force_buffer_release();
    f.open(OUTPUTFILE);

    f << "{\n" <<
      "  \"end\":" << t_bank->ins_next << ",\n" <<
      "  \"sample-size\":" << t_bank->buffer_capacity << ",\n";

    f << "  \"threads\": [\n";
    int first = 1;
    for(int i = 0; i < MAX_THREADS; i++) {
        int exist = make_status_string(str, i);

        if(exist > 0) {
            if(first > 0) {
                first = 0;
            } else {
                f << ",\n";
            }

            f << "    {\n" <<
              "      \"pin-tid\":" << i << ",\n" <<
              "      \"start\":" << t_bank->ins_start[i] << ",\n" <<
              "      \"samples\":\"" << str << "\"\n" <<
              "    }";
        }
    }

    f << "\n  ]\n}\n";
    f.close();
}

// Returns 0 if stayed unregistered the whole time
static int make_status_string(char *s, int index)
{
    int any = 0;
    int start = t_bank->ins_start[index];

    if(start < 0) {
        return 0;
    }

    for(int i = start; i < t_bank->ins_next; i++) {
        s[i - start] = (char)(0x30 + t_bank->bank[i][index]);
        if(t_bank->bank[i][index] != UNREGISTERED) {
            any = 1;
        }
    }

    s[t_bank->ins_next - start] = '\0';
    return any;
}
