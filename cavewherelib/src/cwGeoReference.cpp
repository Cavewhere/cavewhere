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
