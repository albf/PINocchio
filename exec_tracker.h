/* exec_tracker.h
 *
 * Copyright (C) 2017 Alexandre Luiz Brisighello Filho
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef EXEC_TRACKER_H_
#define EXEC_TRACKER_H_

#include "thread.h"

// Init the static heap structures.
void exec_tracker_init();

// Insert a newly added thread, should be waiting.
void exec_tracker_insert(THREAD_INFO *t);

// Try to add a thread to the list. Returns 0 if should stay awake, 1 otherwise.
int exec_tracker_sleep(THREAD_INFO *t);

// Mark a thread as running, move frome one heap to another.
THREAD_INFO *exec_tracker_awake();

// Inform one thread is not running anymore.
void exec_tracker_minus();

// Inform one extra thread running.
void exec_tracker_plus();

//  Return 1 if there is no one running or waiting, 0 otherwise.
int exec_track_is_empty();

// Returns 1 if there is any change since previous call and 0 otherwise.
int exec_tracker_changed();

// Debug function, print what heaps currently hold.
void exec_tracker_print();

#endif // EXEC_TRACKER_H_
