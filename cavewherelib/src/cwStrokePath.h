/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTROKEPATH_H
#define CWSTROKEPATH_H

//Qt includes
#include <QPointF>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"

// Arclength index + nearest-point projector over a world-metre polyline (a sketch
// stroke's path), shared by the placement rules (Uniform spacing, Rotate by
// tangent, Bending stamp) and cwSketchDecorationLayout. Precomputes cumulative
// segment lengths once so each sample is a binary search + lerp rather than an
// O(n) walk. The polyline is borrowed by const reference and must outlive the
// sampler.
//
// The input is already-flattened world-metre points; today strokes are captured
// as polylines (stylus pen sampling). If strokes ever become bezier curves,
// flattening is the single boundary that changes: add a constructor taking a
// QPainterPath and flatten it once via QPainterPath::toSubpathPolygons() into the
// same internal polyline. QPainterPath's own pointAtPercent/percentAtLength are
// Bezier-t parameterised (not arclength-linear when curves are present), so they
// cannot replace this class's arclength sampling; nor does it offer the
// nearest-point projection below. Every downstream rule stays unchanged.
class CAVEWHERE_LIB_EXPORT cwStrokePath {
public:
    explicit cwStrokePath(const QVector<QPointF> &points);

    double totalLength() const { return m_totalLength; }
    bool   isEmpty() const { return m_points.size() < 2; }

    // Point on the polyline at arclength s (world metres), clamped to [0, length].
    QPointF pointAtArcLength(double s) const;

    // Unit tangent at arclength s (direction of travel). Zero-length polylines
    // return (1, 0). Clamped like pointAtArcLength.
    QPointF tangentAtArcLength(double s) const;

    // Left normal at arclength s: tangent rotated +90deg (world +X -> world +Y).
    QPointF normalAtArcLength(double s) const;

    // Point and left normal at arclength s in one segment lookup. The arclength
    // warp samples both at the same s per glyph point, so this shares the single
    // binary search instead of paying for pointAtArcLength + normalAtArcLength.
    void pointAndNormalAtArcLength(double s, QPointF &point, QPointF &normal) const;

    // Unit tangent of the polyline segment nearest worldPoint. Exact for points
    // sampled onto the polyline (e.g. stamp anchors seeded by a Generate rule),
    // letting MutatePerLayer rules orient a stamp without tracking its arclength.
    QPointF tangentNearPoint(const QPointF &worldPoint) const;

    // Arclength of the polyline point nearest worldPoint. Recovers a stamp's
    // arclength from its world anchor for BendingStamp re-parameterisation.
    double arcLengthNearPoint(const QPointF &worldPoint) const;

private:
    // Returns the segment index containing arclength s and the local fraction t
    // in [0, 1] along that segment.
    void locate(double s, int &segment, double &t) const;

    // Returns the index of the segment closest to worldPoint and the local
    // fraction t in [0, 1] of the projection onto it.
    int closestSegment(const QPointF &worldPoint, double &t) const;

    const QVector<QPointF> &m_points;
    QVector<double> m_cumulative;   // cumulative arclength at each point; size == points
    double m_totalLength = 0.0;
};

#endif // CWSTROKEPATH_H
