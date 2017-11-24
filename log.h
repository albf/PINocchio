#ifndef LOG_H_
#define LOG_H_

// Turn on several logs outputs
// #define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG(WHAT) WHAT
#else
#define DEBUG(WHAT) do { } while(0)
#endif

#include "pin.H"

void log_init(THREADID watcher);

// Due to watcher, exposed thread id should be different
// than internal. Provide a converter.
THREADID print_id(THREADID tid);

// Function used to when instrumentation should stop
void fail();

#endif // LOG_H_
