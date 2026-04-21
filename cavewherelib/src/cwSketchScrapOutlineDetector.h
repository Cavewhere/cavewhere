/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHSCRAPOUTLINEDETECTOR_H
#define CWSKETCHSCRAPOUTLINEDETECTOR_H

//Qt includes
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenStroke.h"
#include "cwSketchScrapOutline.h"

// Pure, stateless conversion of pen strokes into closed scrap outlines.
//
// Only Kind::Wall and Kind::ScrapOutline strokes are considered; Feature
// strokes are dropped (no cwScrap will ever be derived from them).
//
// Every eligible stroke contributes two endpoints to a greedy nearest-neighbor
// matcher; pairs of endpoints chain strokes head-to-tail into closed rings.
// A single already-closed stroke self-pairs; two or more open strokes whose
// endpoints cluster near each other form a multi-stroke chain. There is no
// distance cutoff — leads on either end of two parallel walls auto-cap into
// a closed ring by default. Users force a specific cap by drawing a short
// `ScrapOutline` stroke at the cap location; because greedy matching picks
// shorter distances first, that short stroke wins naturally.
class CAVEWHERE_LIB_EXPORT cwSketchScrapOutlineDetector {
public:
    // Full-diagnostics entry point. Returns both the emitted outlines and
    // per-stroke rejection diagnostics. Pays the cost of preserving a per-
    // stroke raw polyline copy and populating the rejection vector — use
    // only when a debug overlay or test actually reads the diagnostics.
    static cwSketchScrapDetectResult
    detectWithDiagnostics(const QVector<cwPenStroke> &strokes,
                          double simplifyToleranceMeters,
                          double outsetMeters = 0.0);

    // Fast path: skips the diagnostic copy/rejection bookkeeping entirely.
    // Production callers that don't need rejection tags should use this.
    static QVector<cwSketchScrapOutline>
    detect(const QVector<cwPenStroke> &strokes,
           double simplifyToleranceMeters,
           double outsetMeters = 0.0);

private:
    static cwSketchScrapDetectResult
    detectImpl(const QVector<cwPenStroke> &strokes,
               double simplifyToleranceMeters,
               double outsetMeters,
               bool   collectDiagnostics);
};

#endif // CWSKETCHSCRAPOUTLINEDETECTOR_H
