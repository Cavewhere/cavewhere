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
    cwScenePicker(parent),
    m_datum(cwCoordinateTransform::Wgs84)
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
        disconnect(m_geoReference, &cwGeoReference::localProjectionChanged,
                   this, &cwCoordinatePicker::rebuildLatLonTransform);
    }
    m_geoReference = geoReference;
    if (m_geoReference) {
        connect(m_geoReference, &cwGeoReference::localProjectionChanged,
                this, &cwCoordinatePicker::rebuildLatLonTransform);
    }
    // A stale pick from the old geo-reference would silently misreport
    // coordinates in the new CRS — clear it.
    clearPick();
    rebuildLatLonTransform();
    emit geoReferenceChanged();
}

void cwCoordinatePicker::setDatum(const QString& datum)
{
    // The datum a pick is reported on is one the table names, so a code it
    // doesn't — an empty string, a projected system, a spelling PROJ knows and
    // the table doesn't — reads as WGS84 rather than as itself.
    const QString tableDatum = cwCoordinateSystem::latLonCS(datum);
    const QString& resolved = tableDatum.isEmpty() ? cwCoordinateTransform::Wgs84 : tableDatum;

    if (m_datum == resolved) {
        return;
    }
    m_datum = resolved;
    rebuildLatLonTransform();
    emit datumChanged();
}

bool cwCoordinatePicker::hasCoordinateSystem() const
{
    return m_geoReference && m_geoReference->hasCoordinateSystem();
}

void cwCoordinatePicker::rebuildLatLonTransform()
{
    const QString cs = m_geoReference ? m_geoReference->localCoordinateSystem() : QString();
    if (cs.isEmpty()) {
        m_latLonTransform.reset();
    } else {
        m_latLonTransform = std::make_unique<cwCoordinateTransform>(cs, m_datum);
        if (!m_latLonTransform->isValid()) {
            m_latLonTransform.reset();
        }
    }

    // A frame change without a new pick must still refresh hasCoordinateSystem,
    // so an open popup reflects the current geo-reference.
    emit coordinateSystemChanged();

    // A pick already on screen moves onto the new transform rather than going
    // stale — the numbers under the marker are the ones the frame and datum say.
    updateLatLon();
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
    m_hasPick = true;

    updateLatLon();
}

void cwCoordinatePicker::updateLatLon()
{
    if (!m_hasPick) {
        return;
    }

    m_hasLatLon = false;
    m_latitude = 0.0;
    m_longitude = 0.0;

    if (m_latLonTransform) {
        // PROJ is normalized for visualization in cwCoordinateTransform's
        // constructor, so output is x=lon, y=lat.
        const cwGeoPoint latLon =
            m_latLonTransform->transform(cwGeoPoint::fromSceneLocal(m_scenePoint));
        m_longitude = latLon.x;
        m_latitude = latLon.y;
        m_hasLatLon = true;
    }

    emit pickChanged();
}

void cwCoordinatePicker::clearPick()
{
    if (!m_hasPick) {
        return;
    }
    m_hasPick = false;
    m_hasLatLon = false;
    emit pickChanged();
}
