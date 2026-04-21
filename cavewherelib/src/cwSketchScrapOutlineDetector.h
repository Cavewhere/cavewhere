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
// strokes and anything that does not form a simple closed polygon are
// dropped (no cwScrap will ever be derived from them).
class CAVEWHERE_LIB_EXPORT cwSketchScrapOutlineDetector {
public:
    static QVector<cwSketchScrapOutline>
    detect(const QVector<cwPenStroke> &strokes,
           double closeThresholdMeters,
           double simplifyToleranceMeters,
           double outsetMeters = 0.0);
};

#endif // CWSKETCHSCRAPOUTLINEDETECTOR_H
