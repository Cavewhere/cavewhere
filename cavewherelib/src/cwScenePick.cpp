/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwScenePick.h"

//Our includes
#include "cwCamera.h"
#include "cwGeometryItersecter.h"
#include "cwLinePlotStationSnap.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"
#include "cwRenderLinePlot.h"

//Qt 3D
#include <qray3d.h>

cwScenePick::Result cwScenePick::snappedPoint(QPointF screenPoint,
                                              const cwCamera& camera,
                                              cwGeometryItersecter& intersecter,
                                              double pixelRadius)
{
    Result result;

    const QRay3D ray = camera.rayFromQtViewport(screenPoint);
    const cwPickQuery query = camera.pickQuery(pixelRadius);

    const cwRayHit hit = intersecter.intersectsDetailed(ray, query);
    if (!hit.hit()) {
        return result;
    }

    result.hit = true;
    result.world = hit.pointWorld();

    // When the pick lands on the centerline, clamp it to the nearest survey
    // station if the cursor is within the pick radius of one. Stations are the
    // line vertices, so a line hit (and only a line hit) can snap — triangle and
    // point hits cast to nullptr and keep their exact surface point.
    if (auto* linePlot = qobject_cast<cwRenderLinePlot*>(hit.object())) {
        if (const auto endpoints = linePlot->segmentEndpoints(hit.firstIndex())) {
            const auto snap = cwLinePlotStationSnap::snapToStation(
                        screenPoint,
                        camera,
                        {endpoints->first, hit.firstIndex()},
                        {endpoints->second, hit.firstIndex() + 1},
                        hit.pointWorld(),
                        pixelRadius);
            result.world = snap.worldPos;
            result.snappedToStation = snap.snapped;
            result.stationVertexIndex = snap.stationVertexIndex;
        }
    }

    return result;
}
