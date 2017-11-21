#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "thread.h"
#include "exec_tracker.h"

struct ORDERED_LIST {
    THREAD_INFO * start;
    THREAD_INFO * end;

    int running;
    UINT64 ins_max;
};

ORDERED_LIST waiting_list;

// Init list (static initialized). Compare function should
// be used to define what thread should be awaked first.

void exec_tracker_init()
{
    waiting_list.start = NULL;
    waiting_list.end = NULL;

    waiting_list.running = 0;
    waiting_list.ins_max = 0;
}


// Exposed API, used by sync. They are just simple operations using
// the list. Tracker shouldn't unlock or do anything but keep track on
// what threads should (if any) be awake next or are waiting.

void exec_tracker_insert(THREAD_INFO *t)
{
    // Empty list.
    if (waiting_list.start == NULL) {
        t->waiting_previous = NULL;
        t->waiting_next = NULL;

        waiting_list.start = t;
        waiting_list.end = t;
        
        return;
    }

    // At least someone there. Begin searching from the back.    
    for(THREAD_INFO *c = waiting_list.end; c != NULL; c = c->waiting_previous) {
        // Found it's position, insert there.
        if(t->ins_count > c->ins_count) {
            t->waiting_previous = c;
            t->waiting_next = c->waiting_next;

            // Update, if any, the next guy. If not, guess who is the new end element.
            if (c->waiting_next != NULL) {
                c->waiting_next -> waiting_previous = t;
            } else {
                waiting_list.end = t;
            }
            c->waiting_next = t;
            return;
        }
    }

    // If passed by the whole list, it's still the first. Not cool.
    t->waiting_next = waiting_list.start;
    t->waiting_previous = NULL;

    waiting_list.start->waiting_previous = t;
    waiting_list.start = t;
} 

// Sleeping is basically an insert, but should update running counter.
void exec_tracker_sleep(THREAD_INFO *t)
{
    waiting_list.running--;
    exec_tracker_insert(t);
}

THREAD_INFO *exec_tracker_awake()
{
    // Check if there is at least one, of course.
    if (waiting_list.start == NULL) {
        return NULL;
    }

    // No one is running, guess we could just wake someone.
    if (waiting_list.running == 0) {
        THREAD_INFO *t = waiting_list.start;

        waiting_list.ins_max = t->ins_count;
        waiting_list.start = t->waiting_next;
        if (waiting_list.start != NULL) {
            waiting_list.start->waiting_previous = NULL;
        }

        waiting_list.running++;
        return t;
    }

    // Someone is running. Can't go unless they are in sync.
    if (waiting_list.ins_max >= waiting_list.start->ins_count) {
        THREAD_INFO *t = waiting_list.start;

        waiting_list.start = t->waiting_next;
        if (waiting_list.start != NULL) {
            waiting_list.start->waiting_previous = NULL;
        }

        waiting_list.running++;
        return t;
    }

    // There is someone running and should wait.
    return NULL;
}

void exec_tracker_minus()
{
    waiting_list.running--;
}

// It's empty if there is no one running or waiting.
int exec_track_is_empty()
{
    return ((waiting_list.running == 0) && (waiting_list.start == NULL)) ? 1 : 0;
}

void exec_tracker_print() {
    cerr << "[Exec Tracker] Waiting-List:" << std::endl; 
    cerr << "  -- running: " << waiting_list.running << std::endl; 
    cerr << "  -- ins_max: " << waiting_list.ins_max << std::endl; 
    cerr << "  --    list:"; 
    for (THREAD_INFO *t = waiting_list.start; t != NULL; t = t->waiting_next) {
        cerr << " [tid: " << t->pin_tid;
        cerr << ", ins_count: " << t->ins_count << "]"; 
    }
    cerr << std::endl;
}
