#ifndef KNOB_H_
#define KNOB_H_

#include "pin.H"

#define DEFAULT_OUTPUT_FILE "trace.json"
#define DEFAULT_TIME_BASED "0"

void knob_welcome();
INT32 knob_usage();

// Expose arguments, so other layers files can access it.

extern KNOB<string> knob_output_file;
extern KNOB<bool> knob_time_based;

#endif // KNOB_H_
