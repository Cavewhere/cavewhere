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
    static QVector<cwSketchScrapOutline>
    detect(const QVector<cwPenStroke> &strokes,
           double simplifyToleranceMeters,
           double outsetMeters = 0.0);
};

#endif // CWSKETCHSCRAPOUTLINEDETECTOR_H
