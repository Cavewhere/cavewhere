/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMARKERSOURCE_H
#define CWMARKERSOURCE_H

//Qt includes
#include <QList>
#include <QObject>
#include <QPointF>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwMarker.h"

/**
 * @brief The list of markers a cwMarkerView draws.
 *
 * A subclass reads its markers from the project (leads, entrances) and edits
 * the list through the protected mutators. Each mutator emits one ranged
 * signal, and countChanged when the count moves, so a whole-tree walk reaches
 * the view as one signal and never as one per marker.
 */
class CAVEWHERE_LIB_EXPORT cwMarkerSource : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MarkerSource)
    QML_UNCREATABLE("MarkerSource is abstract; use LeadMarkerSource or EntranceMarkerSource")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit cwMarkerSource(QObject* parent = nullptr);

    int count() const;
    const cwMarker& marker(int row) const;
    const QList<cwMarker>& markers() const;

    //! The content point of the sprite that sits on a marker's position,
    //! normalized in content space (y-down), as cwRenderSpriteBillboards::setAnchor.
    virtual QPointF spriteAnchor() const;

    //! How far a marker's sprite and text pull toward the viewer, in world units.
    //! Occlusion picking uses it as its slack.
    virtual float depthBias() const;

signals:
    void markersReset();
    void markersInserted(int first, int last);
    void markersRemoved(int first, int last);
    void markersChanged(int first, int last);
    void countChanged();

protected:
    void resetMarkers(const QList<cwMarker>& markers);
    void insertMarkers(int row, const QList<cwMarker>& markers);
    void removeMarkers(int first, int last);
    void replaceMarkers(int first, const QList<cwMarker>& markers);

private:
    QList<cwMarker> m_markers;
};

#endif // CWMARKERSOURCE_H
