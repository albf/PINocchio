#ifndef KNOB_H_
#define KNOB_H_

#include "pin.H"

#define DEFAULT_OUTPUT_FILE "trace.json"
#define DEFAULT_TIME_BASED "0"
#define DEFAULT_SYNC_PERIOD "1"

void knob_welcome();
INT32 knob_usage();

// Expose arguments, so other layers files can access it.

extern KNOB<string> knob_output_file;
extern KNOB<bool> knob_time_based;
extern KNOB<int> knob_sync_frenquency;

#endif // KNOB_H_
