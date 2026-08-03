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

void cwGeoReference::anchorTo(const Anchor& anchor, const QString& localCS)
{
    if (!anchor.isValid() || localCS.isEmpty()) {
        // Half of an anchoring is worse than none: it would leave a state that
        // names an input the frame doesn't come from, and the next edit to that
        // input would move the origin for no reason.
        return;
    }

    setLocalProjection({Anchored, localCS, anchor});
}

void cwGeoReference::freeze()
{
    if (m_localProjection.coordinateSystem.isEmpty()) {
        return;
    }

    setLocalProjection({Frozen, m_localProjection.coordinateSystem, Anchor{}});
}

void cwGeoReference::clear()
{
    setLocalProjection(LocalProjectionState{});
}

void cwGeoReference::restore(State state, const QString& localCS, const Anchor& anchor,
                             const QString& verticalDatum)
{
    LocalProjectionState restored{state, localCS, anchor};

    // A state and a string that disagree can only come from a file written by
    // something else, or by a version that meant something different by them.
    // Repair to the reading that keeps the invariants rather than refusing to
    // load: a project with no frame still opens, it just isn't georeferenced.
    if (restored.coordinateSystem.isEmpty()) {
        restored = LocalProjectionState{};
    } else if (restored.state != Anchored || !restored.anchor.isValid()) {
        // A frame with nothing left to follow is frozen, whatever the file
        // called it — and a state that isn't following an anchor has no use for
        // the one it named.
        restored = LocalProjectionState{Frozen, restored.coordinateSystem, Anchor{}};
    }

    setLocalProjection(restored);
    setVerticalDatum(verticalDatum);
}

void cwGeoReference::setVerticalDatum(const QString& datum)
{
    if (m_verticalDatum == datum) {
        return;
    }
    m_verticalDatum = datum;
    emit verticalDatumChanged();
}

void cwGeoReference::setLocalProjection(const LocalProjectionState& state)
{
    if (m_localProjection == state) {
        return;
    }
    m_localProjection = state;
    emit localProjectionChanged();
}
