/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFarFromOriginNotice.h"
#include "cwCavingRegion.h"
#include "cwGeoReference.h"
#include "cwStationPositionLookup.h"

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>

cwFarFromOriginNotice::cwFarFromOriginNotice(cwCavingRegion* region) :
    QObject(region),
    m_region(region)
{
    connect(m_region, &cwCavingRegion::positionsChanged, this, [this]() {
        setDistanceFromOrigin(solvedCenterDistance(m_region->stationPositions()));
    });
    connect(m_region->geoReference(), &cwGeoReference::localProjectionChanged,
            this, &cwFarFromOriginNotice::updateShown);
}

double cwFarFromOriginNotice::solvedCenterDistance(const cwStationPositionLookup& positions)
{
    const QMap<QString, QVector3D> stations = positions.positions();
    if (stations.isEmpty()) {
        return 0.0;
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    for (const QVector3D& position : stations) {
        minX = std::min(minX, static_cast<double>(position.x()));
        minY = std::min(minY, static_cast<double>(position.y()));
        maxX = std::max(maxX, static_cast<double>(position.x()));
        maxY = std::max(maxY, static_cast<double>(position.y()));
    }

    return std::hypot((minX + maxX) / 2.0, (minY + maxY) / 2.0);
}

void cwFarFromOriginNotice::setDistanceFromOrigin(double distance)
{
    if (m_distanceFromOrigin == distance) {
        return;
    }
    m_distanceFromOrigin = distance;
    emit distanceFromOriginChanged();
    updateShown();
}

void cwFarFromOriginNotice::setShown(bool shown)
{
    if (m_shown == shown) {
        return;
    }
    m_shown = shown;
    emit shownChanged();
}

void cwFarFromOriginNotice::updateShown()
{
    setShown(!m_region->geoReference()->hasCoordinateSystem()
             && m_distanceFromOrigin > kFarFromOriginMeters);
}
