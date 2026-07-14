/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLABELPLACEMENTCONTROL_H
#define CWLABELPLACEMENTCONTROL_H

// Std includes
#include <functional>

// Cancel + progress hooks the export label-placement loops poll so the whole
// stage can run on a worker thread (see cwCaptureViewport) without the
// placement code depending on QtConcurrent / QPromise directly.
//
// The loops call isCanceled() once per label and bail out early when it
// returns true (so a runaway or huge export is abortable mid-run rather than
// only between passes), and call labelProcessed() once per label so the driver
// can report progress. Both hooks are optional: a default-constructed control
// (empty std::function) leaves the loops running to completion with no
// reporting, which is what the non-threaded call sites and tests use.
struct cwLabelPlacementControl {
    std::function<bool()> isCanceled;      // return true to stop placing further labels
    std::function<void()> labelProcessed;  // invoked once per label, before its placement
};

#endif // CWLABELPLACEMENTCONTROL_H
