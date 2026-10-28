/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwMarkerSource.h"
#include "cwRenderSpriteBillboards.h"

cwMarkerSource::cwMarkerSource(QObject* parent) :
    QObject(parent)
{
}

int cwMarkerSource::count() const
{
    return int(m_markers.size());
}

const cwMarker& cwMarkerSource::marker(int row) const
{
    return m_markers.at(row);
}

const QList<cwMarker>& cwMarkerSource::markers() const
{
    return m_markers;
}

QPointF cwMarkerSource::spriteAnchor() const
{
    return cwRenderSpriteBillboards::kDefaultAnchor;
}

float cwMarkerSource::depthBias() const
{
    return 0.0f;
}

void cwMarkerSource::resetMarkers(const QList<cwMarker>& markers)
{
    const int oldCount = count();
    m_markers = markers;
    emit markersReset();
    if(oldCount != count()) {
        emit countChanged();
    }
}

void cwMarkerSource::insertMarkers(int row, const QList<cwMarker>& markers)
{
    if(markers.isEmpty()) {
        return;
    }
    Q_ASSERT(row >= 0 && row <= count());
    m_markers.insert(row, markers.size(), cwMarker());
    for(int i = 0; i < markers.size(); ++i) {
        m_markers[row + i] = markers.at(i);
    }
    emit markersInserted(row, row + int(markers.size()) - 1);
    emit countChanged();
}

void cwMarkerSource::removeMarkers(int first, int last)
{
    if(first > last) {
        return;
    }
    Q_ASSERT(first >= 0 && last < count());
    m_markers.remove(first, last - first + 1);
    emit markersRemoved(first, last);
    emit countChanged();
}

void cwMarkerSource::replaceMarkers(int first, const QList<cwMarker>& markers)
{
    if(markers.isEmpty()) {
        return;
    }
    Q_ASSERT(first >= 0 && first + markers.size() <= count());
    for(int i = 0; i < markers.size(); ++i) {
        m_markers[first + i] = markers.at(i);
    }
    emit markersChanged(first, first + int(markers.size()) - 1);
}
