/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHSTROKEGEOMETRY_H
#define CWSKETCHSTROKEGEOMETRY_H

#include <QPainterPath>
#include <QVector>

#include "CaveWhereLibExport.h"
#include "cwPenPoint.h"

namespace cwSketchStrokeGeometry {

struct Params {
    double maxHalfWidth = 3.0;
    double minHalfWidth = 0.25;
    double widthScale   = 1.5;
    int    endPointTessellation = 5;
};

// Pure builder: thread-safe, no Qt thread-affinity dependencies. Matches the
// original cwSketchPainterPathModel geometry exactly — constant-width strokes
// become line paths (driven by a pen), variable-width strokes are tessellated
// into a closed filled polygon (top edge → end cap → bottom edge).
CAVEWHERE_LIB_EXPORT
void buildPath(QPainterPath &out,
               const QVector<cwPenPoint> &points,
               double width,
               const Params &params = {});

} // namespace cwSketchStrokeGeometry

#endif // CWSKETCHSTROKEGEOMETRY_H
