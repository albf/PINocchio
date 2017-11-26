#include "pin.H"
#include "knob.h"
#include <iostream>

KNOB<string> knob_output_file(KNOB_MODE_WRITEONCE, "pintool", "o", DEFAULT_OUTPUT_FILE, "specify output filename");
KNOB<BOOL> knob_time_based(KNOB_MODE_WRITEONCE, "pintool", "t", DEFAULT_TIME_BASED, "perform time-based evaluation (no-pram)");
KNOB<int> knob_sync_frenquency(KNOB_MODE_WRITEONCE, "pintool", "p", DEFAULT_SYNC_PERIOD, "only sync on a given frenquency");

void knob_welcome()
{
    cerr <<  "===============================================" << std::endl;
    cerr << " _ __ (_)_ __   ___   ___ ___| |__ (_) ___\n\
| \'_ \\| | \'_ \\ / _ \\ / __/ __| \'_ \\| |/ _ \\ \n\
| |_) | | | | | (_) | (_| (__| | | | | (_) | \n\
| .__/|_|_| |_|\\___/ \\___\\___|_| |_|_|\\___/ \n\
|_|\n";
    cerr <<  "    Application instrumented by PINocchio" << std::endl;
    cerr <<  "===============================================" << std::endl;
}

INT32 knob_usage()
{
    cerr << "PINocchio is a MIMD PRAM simulator, keeping all threads always sinced by the number of instructions." << std::endl;
    cerr << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}
