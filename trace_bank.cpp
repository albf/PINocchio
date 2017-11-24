#include "trace_bank.h"
#include "log.h"
#include "knob.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>

#define FILTER_SIZE (REDUCTION_SIZE+1)/2

static P_TRACE * traces [MAX_THREADS];

// For the timed-version
// struct timespec start;
static struct timeval start;
static int pram;

void trace_bank_init(int pram_)
{
    if (pram_ == 0) {
        gettimeofday(&start, NULL);
    }
    pram = pram_;

    for(int i = 0; i < MAX_THREADS; i++) {
        traces[i] = NULL;
    }
    DEBUG(cerr << "[Trace Bank] Bank Initiated" << std::endl);
}

// Filter will remove some of the smaller traces available It will look for small pairs
// (can't remove only one, or will "change to the same state").
static void trace_bank_filter(THREADID tid)
{
    // List containing the indexex of interest.
    // If here, should remove i+1 and i+2
    int index[FILTER_SIZE];
    UINT64 value[FILTER_SIZE];

    for (int i = 0; i < FILTER_SIZE; i++) {
        index[i] = -1;
        value[i] = 0;
    }

    for (int i = 0; i < (MAX_BANK_SIZE-4); i++) {
        if (traces[tid]->changes[i].status == traces[tid]->changes[i+3].status) {
            continue;
        }

        UINT64 diff = traces[tid]->changes[i+3].time - traces[tid]->changes[i].time;
        int good = 0;

        // Insert in a insertion-sort style way.
        for (int j = 0; j < FILTER_SIZE; j++) {
            if ((value[j] == 0) || (value[j] < diff)) {
                // Swap if not first
                UINT64 r = value[j];
                int s = index[j];

                value[j] = diff;
                index[j] = i;

                if(j > 0) {
                    value[j-1] = r;
                    index[j-1] = s;
                }

                good = 1;
            } else {
                break;
            }
        }

        //                                                    0      1      2      3      4      5      6
        // If taking it, can't take the next two. Reasoning: (U) -> (L) -> (U) -> (L) -> (U) -> (L) -> (U)
        //  Removing 1, 2, 3 and 4 will result in a bigger hole. Removing 1, 2, 4 and 5 will not.
        if (good > 0) {
            i = i + 2;
        }
    }

    // Elements to be removed were selected. Mark them.
    for (int i = 0; i < FILTER_SIZE; i++) {
        if (index[i] >= 0) {
            traces[tid]->changes[index[i]+1].status = UNREGISTERED;
            traces[tid]->changes[index[i]+2].status = UNREGISTERED;
        }
    }

    // Remove the not used values. Notice it's not pretty but shouldn't happen often.
    int jump = 0;
    for (int i = 0; i < MAX_BANK_SIZE; i++) {
        if (traces[tid]->changes[i].status == UNREGISTERED) {
            jump++;
        } else {
            traces[tid]->changes[i-jump].status = traces[tid]->changes[i].status;
            traces[tid]->changes[i-jump].time   = traces[tid]->changes[i].time;
        }
    }

    // Total_changes should also be updated, and here the reduction/filter is completed.
    traces[tid]->total_changes = traces[tid]->total_changes - REDUCTION_SIZE;
}

void trace_bank_validate()
{
    for(int i = 0; i < MAX_THREADS; i++) {
        P_TRACE *tr = traces[i];
        if (tr != NULL) {
            THREAD_STATUS s = tr->changes[0].status;
            UINT64 t = tr->changes[0].time;
            for(int j = 1; j < tr->total_changes; j++) {
                if (s == tr->changes[j].status) {
                    cerr << "[Trace Bank] Status Error on time: " << tr->changes[j].time << std::endl;
                    trace_bank_print();
                }
                if (t > tr->changes[j].time) {
                    cerr << "[Trace Bank] Time Error on time: " << tr->changes[j].time << std::endl;
                    trace_bank_print();
                }

                s = tr->changes[j].status;
                t = tr->changes[j].time;
            }
        }
    }
}

static UINT64 diff_msec()
{
    struct timeval stop;
    gettimeofday(&stop, NULL);

    return UINT64((stop.tv_sec - start.tv_sec) * 1000 + (stop.tv_usec - start.tv_usec) / 1000);
}

void trace_bank_update(THREADID tid, UINT64 time, THREAD_STATUS status)
{
    int n = traces[tid]->total_changes;
    if (n >= MAX_BANK_SIZE) {
        // Warning: Size will change after bank filter.
        trace_bank_filter(tid);
        n = traces[tid]->total_changes;
    }

    if (pram > 0) {
        traces[tid]->changes[n].time = time;
    } else {
        // Only used for time-based, no PRAM mode.
        traces[tid]->changes[n].time = (UINT64) diff_msec();
    }
    traces[tid]->changes[n].status = status;

    traces[tid]->total_changes++;
}

void trace_bank_register(THREADID tid, UINT64 time)
{
    DEBUG(cerr << "[Trace Bank] Register: " << tid << std::endl);
    if (traces[tid] != NULL) {
        free(traces[tid]);
    }

    traces[tid] = (P_TRACE *) malloc (sizeof(P_TRACE));

    if (pram > 0) {
        traces[tid]->start = time;
    } else {
        traces[tid]->start = diff_msec();
    }
    traces[tid]->end = 0;
    traces[tid]->total_changes = 0;
    traces[tid]->changes = (CHANGE *) malloc (MAX_BANK_SIZE*sizeof(CHANGE));

    trace_bank_update(tid, time, UNLOCKED);
}

void trace_bank_finish(THREADID tid, UINT64 time)
{
    trace_bank_update(tid, time, FINISHED);
    if (pram > 0) {
        traces[tid]->end = time;
    } else {
        traces[tid]->end = diff_msec();
    }
}

static UINT64 find_end() {
    UINT64 max = 0;
    for(int i = 0; i < MAX_THREADS; i++) {
        if (traces[i] != NULL) {
            if (traces[i]->end < 1) {
                cerr << "[PINocchio] Warning: Trace dump before thread exit: " << print_id(i) << std::endl;
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

    DEBUG(cerr << "[Trace Bank] Dumping report to " << knob_output_file.Value() << std::endl);
    f.open(knob_output_file.Value().c_str());

    f << "{\n" <<
      "  \"end\":" << find_end() << ",\n";
      if (pram > 0) {
        f << "  \"unit\": \"Cycles\",\n";
      } else {
        f << "  \"unit\": \"ms\",\n";
      }

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
              "      \"pin-tid\":" << print_id(i) << ",\n" <<
              "      \"start\":" << traces[i]->start << ",\n" <<
              "      \"samples\":" << str << "\n" <<
              "    }";
        }
    }

    f << "\n  ]\n}\n";
    f.close();
}

void trace_bank_free()
{
    for(int i = 0; i < MAX_THREADS; i++) {
        if (traces[i] != NULL) {
            free(traces[i]);
        }
    }
};

void trace_bank_print()
{
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
