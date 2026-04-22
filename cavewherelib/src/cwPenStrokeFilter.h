/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPENSTROKEFILTER_H
#define CWPENSTROKEFILTER_H

//Qt includes
#include <QLoggingCategory>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenPoint.h"

// Enable at runtime with:  QT_LOGGING_RULES="cw.sketch.penfilter=true"
CAVEWHERE_LIB_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcPenFilter)

// Trims Apple Pencil-style "hook" artifacts — the short direction-reversal
// spur that appears when the pen tip slips while pressure ramps up on touch-
// down (and, symmetrically, on lift-off). Well-behaved strokes pass through
// unchanged: the trim only fires when a sharp reversal sits within a tiny
// absolute radius of the stroke endpoint.
namespace cwPenStrokeFilter {

struct CAVEWHERE_LIB_EXPORT Params {
    // Absolute upper bound on reversal-arm length, in **world meters** —
    // the same units as cwPenPoint::position. The user-facing setting
    // is in physical millimetres on the input surface; callers
    // (cwSketchSettings::penStrokeFilterParams) convert via the current
    // map scale and view zoom, so the cap is zoom/scale invariant from
    // the user's perspective.
    double maxHookLength = 0.050;

    // Relative bound: hook arm must also be below this fraction of the
    // stroke's total path length. This is the guard that protects
    // deliberate short strokes — a 2 cm tick with a 1 cm reversal has
    // arm/body = 50% and won't be trimmed. A real hook on a long stroke
    // is typically a few percent of the body. The effective cap on each
    // call is `min(maxHookLength, totalPathLength * maxHookFraction)`.
    double maxHookFraction = 0.15;

    // Two-stage detection:
    //
    // (1) V-reversal stage: a sharp single-vertex reversal between two
    //     consecutive direction vectors. `minReversalAngleDeg` is the
    //     minimum angle between them. This catches classical V-hooks
    //     including tail hooks on long strokes, where the whole scan
    //     window is inside the hook region.
    double minReversalAngleDeg = 110.0;

    // (2) Stable-direction stage (only tried if V-reversal doesn't
    //     fire): a leading sample is "in the hook" when its
    //     displacement from the endpoint points more than
    //     `hookMisalignDeg` off the stroke's stable direction. This
    //     catches L-shaped touchdowns where the pen drifts sideways
    //     for a couple of samples before the real direction emerges —
    //     the per-vertex angles are only 90° so V-reversal misses it,
    //     but the overall displacement stays misaligned.
    double hookMisalignDeg = 60.0;

    // Max number of *unique* samples scanned from each end. The walker
    // skips duplicate coordinates that iOS/iPadOS stylus streams emit
    // while the tip is momentarily stationary, so this counts real
    // direction changes, not raw samples.
    int maxWindow = 12;

    // Minimum inter-sample distance treated as a real motion (world
    // units). Consecutive samples closer than this are folded into one
    // when searching for a reversal. Apple Pencil emits 2–3 duplicates
    // per real sample during press-down; 10 µm is well below any real
    // pen move but comfortably above float rounding.
    double dedupeEpsilon = 1e-5;
};

CAVEWHERE_LIB_EXPORT QVector<cwPenPoint> trimHooks(const QVector<cwPenPoint> &points,
                                                   const Params &params = {});

}

#endif // CWPENSTROKEFILTER_H
