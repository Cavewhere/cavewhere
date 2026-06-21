/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGeoReference.h"

cwGeoReference::cwGeoReference(QObject* parent) :
    QObject(parent)
{
}

void cwGeoReference::setGlobalCoordinateSystem(const QString& cs)
{
    if (m_globalCoordinateSystem == cs) {
        return;
    }
    m_globalCoordinateSystem = cs;
    // The stored worldOrigin was computed in the old CS, so reset it. The next
    // line-plot completion will auto-recompute against the new CS. Reset both
    // value and explicitlySet directly — a CS-driven reset is not a user choice,
    // so leaving the flag false lets a freshly added LAZ seed the origin from
    // its bbox center again.
    if (m_worldOrigin.value != cwGeoPoint{} || m_worldOrigin.explicitlySet) {
        m_worldOrigin = WorldOriginState{};
        emit worldOriginChanged();
    }
    emit globalCoordinateSystemChanged();
}

void cwGeoReference::setWorldOrigin(const cwGeoPoint& origin)
{
    // Short-circuit only when both the value AND the explicit-set flag are
    // already what we'd produce. An explicit setWorldOrigin(0,0,0) on a freshly
    // constructed reference must still flip the flag — otherwise the user's
    // intent is indistinguishable from "never set" and LAZ auto-adopt will
    // silently overwrite it (see [cwSinkTrainingModel] failures).
    if (m_worldOrigin.value == origin && m_worldOrigin.explicitlySet) {
        return;
    }
    m_worldOrigin.value = origin;
    m_worldOrigin.explicitlySet = true;
    emit worldOriginChanged();
}
