#ifndef CWMARKERKNOBSRESET_H
#define CWMARKERKNOBSRESET_H

//Our includes
#include "cwRenderingSettings.h"

//! Puts the marker knobs at their defaults when a test starts and on every path
//! out of it: the settings singleton outlives every case in the process.
class cwMarkerKnobsReset {
public:
    cwMarkerKnobsReset() { reset(); }
    ~cwMarkerKnobsReset() { reset(); }

    cwMarkerKnobsReset(const cwMarkerKnobsReset&) = delete;
    cwMarkerKnobsReset& operator=(const cwMarkerKnobsReset&) = delete;

private:
    static void reset()
    {
        if (auto* settings = cwRenderingSettings::instance()) {
            settings->resetMarkerKnobsToDefaults();
        }
    }
};

#endif // CWMARKERKNOBSRESET_H
