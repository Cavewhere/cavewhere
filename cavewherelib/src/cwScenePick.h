/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCENEPICK_H
#define CWSCENEPICK_H

//Our includes
#include "cwPickQuery.h"

//Qt includes
#include <QPointF>
#include <QVector3D>

class cwCamera;
class cwGeometryItersecter;

//! Shared ray-cast-and-snap pick against the scene geometry.
/*!
    Ray-casts a Qt-viewport screen point against the intersecter, then clamps
    the hit to the nearest survey station when it lands on the centerline and
    the cursor is within linePixelRadius of a station vertex.

    With the default kinds (cwPickQuery::All) occlusion is handled for free by
    the depth-ordered BVH: a station hidden behind geometry yields a triangle
    hit first, so it never snaps. A caller that wants stations to pick through
    occluders (the measurement tool's Station-only mode) narrows kinds to
    cwPickQuery::Kind::Lines, which skips every triangle and point sub-BVH so
    the ray reaches the centerline regardless of what is in front of it.

    The coordinate picker and the measurement tool both pick the same way, so
    this is the single seam they share.
*/
namespace cwScenePick {

struct Result {
    QVector3D world;                 //!< snapped station, or the raw hit point
    bool hit = false;                //!< false when the ray missed all geometry
    bool snappedToStation = false;   //!< true when clamped to a station vertex
    int stationVertexIndex = -1;     //!< the snapped vertex, or -1
};

//! Picks screenPoint (Qt viewport pixels) against intersecter through camera.
/*!
    linePixelRadius is a screen-space radius in PIXELS — callers convert it
    from physical millimeters (see cwScenePicker). It governs the centerline
    alone: it sets both the ray-vs-line tolerance and the clamp-to-station
    reach. Triangles pick exactly and points pick against their own world-space
    radius, so raising this widens station snapping and nothing else. See
    cwPickTolerance for why points keep a separate radius.

    kinds narrows which primitive kinds the ray tests. It defaults to
    cwPickQuery::All (nearest of everything by depth wins); pass
    cwPickQuery::Kind::Lines to pick the centerline through any occluding
    geometry.
*/
Result snappedPoint(QPointF screenPoint,
                    const cwCamera& camera,
                    cwGeometryItersecter& intersecter,
                    double linePixelRadius,
                    cwPickQuery::Kinds kinds = cwPickQuery::All);

}

#endif // CWSCENEPICK_H
