#ifndef ERROR_H_
#define ERROR_H_

// Turn on several logs outputs
#define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG(WHAT) WHAT
#else
#define DEBUG(WHAT) do { } while(0)
#endif

void fail();

#endif // ERROR_H_ 
