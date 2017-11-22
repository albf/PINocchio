#ifndef KNOB_H_
#define KNOB_H_

#include "pin.H"

#define DEFAULT_OUTPUT_FILE "trace.json"

void knob_welcome();
INT32 knob_usage();

// Expose arguments, so other layers files can access it.

extern KNOB<string> knob_output_file;

#endif // KNOB_H_ 
