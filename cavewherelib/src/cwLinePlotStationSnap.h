/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTSTATIONSNAP_H
#define CWLINEPLOTSTATIONSNAP_H

// Qt includes
#include <QVector3D>
#include <QPointF>

class cwCamera;

namespace cwLinePlotStationSnap {

    // A centerline vertex: its world position paired with its vertex index. The
    // hit segment's two endpoints are passed as these (the from and to stations).
    struct SegmentStation {
        QVector3D position;
        int vertexIndex = -1;
    };

    struct StationSnap {
        QVector3D worldPos;
        int stationVertexIndex = -1;
        bool snapped = false;
    };

    // Pure screen-space clamp-to-station for a centerline pick. Projects the hit
    // segment's two endpoint stations to Qt viewport pixels; if the one nearest
    // the cursor in screen space is within pixelRadius, returns its world position
    // and vertex index. Otherwise returns freePointWorld (the free point on the
    // segment, e.g. cwRayHit::pointWorld) unsnapped.
    //
    // An endpoint behind the eye or off-screen is ineligible — a point behind the
    // camera survives the perspective divide with a flipped sign and could land
    // near the cursor by accident, so it must never win. Ties in pixel distance
    // resolve to the lower vertex index for determinism.
    //
    // Pure and camera-aware: no BVH or render-object coupling, so it tests with
    // just a cwCamera and two points.
    StationSnap snapToStation(QPointF cursorPx,
                              const cwCamera& camera,
                              SegmentStation from,
                              SegmentStation to,
                              QVector3D freePointWorld,
                              double pixelRadius);
}

#endif // CWLINEPLOTSTATIONSNAP_H
