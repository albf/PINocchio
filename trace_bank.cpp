#include "trace_bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

P_TRACE * traces [MAX_THREADS];

void trace_bank_init()
{
    for(int i = 0; i < MAX_THREADS; i++) {
        traces[i] = NULL;
    }
    cerr << "[Trace Bank] Bank Initiated" << std::endl;
}

void trace_bank_filter(THREADID tid) {
    int index[REDUCTION_SIZE];
    UINT64 value[REDUCTION_SIZE];

    for (int i = 0; i < REDUCTION_SIZE; i++) {
        index[i] = -1;
        value[i] = -1;
    }

    for (int i = 0; i < (MAX_BANK_SIZE-1); i++) {
        UINT64 diff = traces[tid]->changes[i+1].time - traces[tid]->changes[i].time;

        if (i == 0 && diff < value[0]) {
            value[0] = diff;
            index[0] = tid;
        }

        // Insert in a insertion-sort style.
        for (int j = 1; j < REDUCTION_SIZE; j++) {
            if (value[j-1] < value[j]) {
                UINT64 r = value[j];
                value[j] = value[j-1];
                value[j-1] = r;

                int s = index[j];
                index[j] = index[j-1];
                index[j-1] = s;
            } else {
                break;
            }
        }
    }

    // Elements to be removed were selected. Mark them.
    for (int i = 0; i < REDUCTION_SIZE; i++) {
        traces[tid]->changes[index[i]].status = UNREGISTERED;
    }

    // Remove the not used values. Notice it's not pretty but shouldn't happen often.
    int jump = 0;
    for (int i = 1; i < MAX_BANK_SIZE; i++) {
        if (traces[tid]->changes[i].status == UNREGISTERED) {
            jump++;
        } else {
            traces[tid]->changes[i-jump].status = traces[tid]->changes[i].status;
            traces[tid]->changes[i-jump].time = traces[tid]->changes[i].time;
        }
    }

    // Total_changes should also be updated, and here the reduction/filter is completed.
    traces[tid]->total_changes = traces[tid]->total_changes - REDUCTION_SIZE;
}

void trace_bank_update(THREADID tid, UINT64 time, THREAD_STATUS status) {
    int n = traces[tid]->total_changes;
    if (n >= MAX_BANK_SIZE) {
        trace_bank_filter(tid);
    }

    traces[tid]->changes[n].time = time;
    traces[tid]->changes[n].status = status;

    traces[tid]->total_changes++;
}

void trace_bank_register(THREADID tid, UINT64 time) {
    cerr << "[Trace Bank] Register: " << tid << std::endl;
    if (traces[tid] != NULL) {
        free(traces[tid]);
    }
    
    traces[tid] = (P_TRACE *) malloc (sizeof(P_TRACE));

    traces[tid]->start = time;
    traces[tid]->end = 0;
    traces[tid]->total_changes = 0;

    trace_bank_update(tid, time, UNLOCKED);
}

void trace_bank_finish(THREADID tid, UINT64 time) {
    trace_bank_update(tid, time, FINISHED);

    traces[tid]->end = time;
}

UINT64 find_end() {
    UINT64 max = 0;
    for(int i = 0; i < MAX_THREADS; i++) {
        if (traces[i] != NULL) {
            if (traces[i]->end < 1) {
                cerr << "Warning: Trace dump before thread exit: " << i << std::endl;
            } else if (traces[i]->end > max) {
                max = traces[i]->end;
            }
        }
    }
    return max;
}

// Returns 0 if stayed unregistered the whole time
static int make_status_string(char *s, int index)
{
    if(traces[index] == NULL) {
        return 0;
    }

    s[0] = '\0';
    sprintf(s,"[");
    for(int i = 0; i < traces[index]->total_changes; i++) {
        if (i > 0) {
            sprintf(s,"%s, ", s);
        }
        char status = (char)(0x30 + traces[index]->changes[i].status);
        sprintf(s,"%s[%lu, %c]", s, traces[index]->changes[i].time, status);
    }
    sprintf(s,"%s]", s);

    return 1;
}

void trace_bank_dump()
{
    char str[32*MAX_BANK_SIZE];
    ofstream f;

    cerr << "[Trace Bank] Dumping report to trace.json" << std::endl;

    f.open(OUTPUTFILE);

    f << "{\n" <<
      "  \"end\":" << find_end() << ",\n";

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
              "      \"start\":" << traces[i]->start << ",\n" <<
              "      \"samples\":\"" << str << "\"\n" <<
              "    }";
        }
    }

    f << "\n  ]\n}\n";
    f.close();
}

void trace_bank_free() {
    for(int i = 0; i < MAX_THREADS; i++) {
        if (traces[i] != NULL) {
            free(traces[i]);
        }
    }
};

void trace_bank_print() {
    cerr << "[Trace Bank] Printing current state" << std::endl;
    for(int i = 0; i < MAX_THREADS; i++) {
        P_TRACE *tr = traces[i];
        if (tr != NULL) {
            cerr << "Thread: " << i << std::endl;
            cerr << "start: " << tr->start << std::endl;
            cerr << "End: " << tr->end << std::endl;
            cerr << "total_changes: " << tr->total_changes << std::endl;

            cerr << "Changes: " << std::endl;
            for(int j = 0; j < tr->total_changes; j++) {
                cerr << "  time:" << traces[i]->changes[j].time << std::endl; 
                cerr << "  status:" << traces[i]->changes[j].status << std::endl; 
                cerr << "-----"<< std::endl; 
            }
            cerr << "--------------------" << std::endl; 
        }
    }
}
