/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCoordinatePicker.h"

//Our includes
#include "cwCoordinateTransform.h"
#include "cwGeoReference.h"
#include "cwScenePick.h"

cwCoordinatePicker::cwCoordinatePicker(QQuickItem* parent) :
    cwScenePicker(parent)
{
}

cwCoordinatePicker::~cwCoordinatePicker() = default;

cwGeoReference* cwCoordinatePicker::geoReference() const
{
    return m_geoReference;
}

void cwCoordinatePicker::setGeoReference(cwGeoReference* geoReference)
{
    if (m_geoReference == geoReference) {
        return;
    }
    if (m_geoReference) {
        disconnect(m_geoReference, &cwGeoReference::globalCoordinateSystemChanged,
                   this, &cwCoordinatePicker::rebuildWgs84Transform);
    }
    m_geoReference = geoReference;
    if (m_geoReference) {
        connect(m_geoReference, &cwGeoReference::globalCoordinateSystemChanged,
                this, &cwCoordinatePicker::rebuildWgs84Transform);
    }
    // A stale pick from the old geo-reference would silently misreport
    // coordinates in the new CRS — clear it.
    clearPick();
    rebuildWgs84Transform();
    emit geoReferenceChanged();
}

bool cwCoordinatePicker::hasCoordinateSystem() const
{
    return m_geoReference && m_geoReference->hasCoordinateSystem();
}

void cwCoordinatePicker::rebuildWgs84Transform()
{
    const QString cs = m_geoReference ? m_geoReference->globalCoordinateSystem() : QString();
    // Keep the cache in sync so the globalCoordinateSystem Q_PROPERTY reflects
    // the current region between picks (the getter reads this cache).
    m_globalCoordinateSystemCached = cs;
    if (cs.isEmpty()) {
        m_wgs84Transform.reset();
    } else {
        m_wgs84Transform = std::make_unique<cwCoordinateTransform>(cs, cwCoordinateTransform::Wgs84);
        if (!m_wgs84Transform->isValid()) {
            m_wgs84Transform.reset();
        }
    }

    // A CS change without a new pick must still refresh the CS-derived readouts
    // (globalCoordinateSystem, hasCoordinateSystem) so an open popup reflects the
    // current geo-reference.
    emit coordinateSystemChanged();
}

void cwCoordinatePicker::pick(QPointF screenPoint)
{
    if (!m_geoReference) {
        return;
    }

    const cwScenePick::Result pick = snapPick(screenPoint);
    if (!pick.hit) {
        return;
    }

    m_scenePoint = pick.world;
    m_pickScreenPoint = screenPoint;

    m_globalPoint = m_geoReference->toGlobal(m_scenePoint);

    m_hasWgs84 = false;
    m_wgs84Lat = 0.0;
    m_wgs84Lon = 0.0;

    if (m_wgs84Transform) {
        // PROJ is normalized for visualization in cwCoordinateTransform's
        // constructor, so output is x=lon, y=lat.
        const cwGeoPoint w = m_wgs84Transform->transform(m_globalPoint);
        m_wgs84Lon = w.x;
        m_wgs84Lat = w.y;
        m_hasWgs84 = true;
    }

    m_hasPick = true;
    emit pickChanged();
}

void cwCoordinatePicker::clearPick()
{
    if (!m_hasPick) {
        return;
    }
    m_hasPick = false;
    m_hasWgs84 = false;
    emit pickChanged();
}
