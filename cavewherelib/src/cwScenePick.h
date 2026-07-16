/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCENEPICK_H
#define CWSCENEPICK_H

//Qt includes
#include <QPointF>
#include <QVector3D>

class cwCamera;
class cwGeometryItersecter;

//! Shared ray-cast-and-snap pick against the scene geometry.
/*!
    Ray-casts a Qt-viewport screen point against the intersecter, then clamps
    the hit to the nearest survey station when it lands on the centerline and
    the cursor is within pixelRadius of a station vertex. Occlusion is handled
    for free by the depth-ordered BVH: a station hidden behind geometry yields a
    triangle hit first, so it never snaps.

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
Result snappedPoint(QPointF screenPoint,
                    const cwCamera& camera,
                    cwGeometryItersecter& intersecter,
                    double pixelRadius);

}

#endif // CWSCENEPICK_H
