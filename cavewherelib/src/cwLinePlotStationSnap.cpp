/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLinePlotStationSnap.h"

// Our includes
#include "cwCamera.h"

// Std includes
#include <optional>

// Qt includes
#include <QMatrix4x4>

namespace cwLinePlotStationSnap {

StationSnap snapToStation(QPointF cursorPx,
                          const cwCamera& camera,
                          SegmentStation from,
                          SegmentStation to,
                          QVector3D freePointWorld,
                          double pixelRadius)
{
    const QMatrix4x4 viewProjection = camera.projectionMatrix() * camera.viewMatrix();

    // Squared pixel distance from the cursor to an endpoint, or nullopt when the
    // endpoint is off-screen / behind the eye (ineligible — its projection is not
    // a trustworthy screen position).
    const auto pixelDistanceSq = [&](const QVector3D& world) -> std::optional<double> {
        const QVector3D ndc = viewProjection.map(world);
        if (cwCamera::isNormalizedDeviceCoordinateClipped(ndc)) {
            return std::nullopt;
        }
        const QPointF px = camera.project(world);
        const double dx = px.x() - cursorPx.x();
        const double dy = px.y() - cursorPx.y();
        return dx * dx + dy * dy;
    };

    const double radiusSq = pixelRadius * pixelRadius;
    const std::optional<double> distFrom = pixelDistanceSq(from.position);
    const std::optional<double> distTo = pixelDistanceSq(to.position);

    const bool fromWithin = distFrom.has_value() && distFrom.value() <= radiusSq;
    const bool toWithin = distTo.has_value() && distTo.value() <= radiusSq;

    if (fromWithin || toWithin) {
        // Nearer endpoint wins; a tie resolves to the lower vertex index.
        const bool pickFrom = fromWithin &&
                (!toWithin
                 || distFrom.value() < distTo.value()
                 || (distFrom.value() == distTo.value() && from.vertexIndex <= to.vertexIndex));
        const SegmentStation& chosen = pickFrom ? from : to;
        return StationSnap{ chosen.position, chosen.vertexIndex, true };
    }

    return StationSnap{ freePointWorld, -1, false };
}

} // namespace cwLinePlotStationSnap
