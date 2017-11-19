#include <stdio.h>
#include <stdlib.h>
#include "thread.h"

#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2

// Store both waiting and running heaps. Use global since they don't have
// to be allocated in runtime. Order should be -1 for Min-Heap and 1 for Max-Heap.
struct HEAP {
    THREAD_INFO * data[MAX_THREADS];
    int size;
    int order;
};

HEAP * waiting_heap;
HEAP * running_heap;



// Init heap and list (static initialized). Compare function should
// be used to define what thread should be awaked first.

void exec_tracker_init() {
    waiting_heap->size = 0;
    running_heap->size = 0;

    waiting_heap->order = -1;
    running_heap->order = 1;

    for(int i = 0; i < MAX_THREADS; i++) {
        waiting_heap->data[i] = NULL;
        running_heap->data[i] = NULL;
    }
}

// Compare two threads and return -1 (a<b), 0 (a==b), or 1 (a>b),
// taking into consideration the number of instructions executed so far.
int compare_threads(THREAD_INFO *t_a, THREAD_INFO *t_b) {
    int i_a, i_b;

    i_a = t_a->ins_count;
    i_b = t_b->ins_count;

    if (i_a == i_b)
        return 0;

    if (i_a > i_b)
        return 1;
    
    return -1;
}

int compare_elem(HEAP *heap, int a, int b) {
    THREAD_INFO *t_a, *t_b;

    t_a = heap->data[a];
    t_b = heap->data[b];

    return  compare_threads(t_a, t_b);
}

// Heap-related functions

void heapify_down(HEAP *heap, int i) {
    // most is max/min(term, left child, right child), depending on heap order.

    int most = i;
    if ((LCHILD(i) < heap->size) && (compare_elem(heap, i, LCHILD(i)) == heap->order)) {
        most = LCHILD(i);
    }
    if ((RCHILD(i) < heap->size) && (compare_elem(heap, i, RCHILD(i)) == heap->order)) {
        most = RCHILD(i);
    }

    if(most != i) {
        // Swap and heapify
        THREAD_INFO * a = heap->data[i];

        heap->data[i] = heap->data[most];
        heap->data[most] = a;

        heapify_down(heap, most);
    }
}

void heapify_up(HEAP *heap, int i) {
    while((i > 0) && (compare_elem(heap, i, PARENT(i)) == heap->order)) {
        // Swap and continue 
        THREAD_INFO *a = heap->data[i];

        heap->data[i] = heap->data[PARENT(i)];
        heap->data[PARENT(i)] = a;
    }
}

void heap_push(HEAP *heap, THREAD_INFO * t) {
    // Append to the last position and heapify.
    heap->data[heap->size] = t;
    heap->size++;
    heapify_up(heap, heap->size-1);
}

THREAD_INFO * heap_pop(HEAP *heap) {
    THREAD_INFO * t;

    // Shouldn't happen.
    if (heap->size <= 0) {
        return NULL;
    }

    // Remove first, put last in there, heapify.
    t = heap->data[0];
    heap->size--;
    heap->data[0] = heap->data[heap->size];
    heapify_down(heap, 0);
    return t;
}

void heap_remove(HEAP *heap, THREAD_INFO *t) {
    // Find it, put the last one there and heapify.
    for (int i = 0; i < heap->size; i++) {
        if(heap->data[i] == t) {
            heap->size--;
            heap->data[i] = heap->data[heap->size];
            heapify_up(heap, i);
            return;
        }
    }
}

// Exposed API, used by sync. They are just simple operations using
// the two chosen heaps. Tracker shouldn't unlock or do anything but
// keep track on what threads should (if any) be awake next or are running.

void exec_tracker_insert(THREAD_INFO *t) {
    heap_push(waiting_heap, t);
}

void exec_tracker_remove(THREAD_INFO *t) {
    heap_remove(running_heap, t);
}

void exec_tracker_sleep(THREAD_INFO *t) {
    heap_remove(running_heap, t);
    heap_push(waiting_heap, t);
}

void exec_tracker_awake() {
    heap_push(running_heap, heap_pop(waiting_heap));
}

THREAD_INFO * exec_tracker_peek_waiting() {
    return waiting_heap->data[0];
}

THREAD_INFO * exec_tracker_peek_running() {
    return running_heap->data[0];
}
