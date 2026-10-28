/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWENTRANCEMARKERSOURCE_H
#define CWENTRANCEMARKERSOURCE_H

//Qt includes
#include <QList>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QUuid>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwMarkerSource.h"
class cwSurveyNode;

/**
 * @brief The entrance markers: one marker per placed fix whose station the
 * solve placed below the fix's node. Every such fix counts as an entrance.
 *
 * A marker's text is its display name in the "name" style and the
 * cwEntranceLabel length and depth line under it in the "stats" style. The
 * display name is the label of the node's entrance entry for the station when
 * it has one, else the name of the nearest enclosing Cave node, else the fix's
 * own node's name. Its keyword item extends the node's keywords with
 * Type = "Entrance" and Name = the display name. Model, solve, name, kind, size
 * and unit changes coalesce into one queued rebuild that emits at most one
 * signal of each kind.
 */
class CAVEWHERE_LIB_EXPORT cwEntranceMarkerSource : public cwMarkerSource
{
    Q_OBJECT
    QML_NAMED_ELEMENT(EntranceMarkerSource)

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)

public:
    explicit cwEntranceMarkerSource(QObject* parent = nullptr);

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

    //! The row of @a node's fix @a fixId, or -1 when that fix of that node has no marker.
    Q_INVOKABLE int row(cwSurveyNode* node, const QUuid& fixId) const;

    QPointF spriteAnchor() const override;
    float depthBias() const override;

signals:
    void regionChanged();

private:
    // What one rebuild pass wants drawn for a fix.
    struct Entrance {
        QUuid fixId;
        cwSurveyNode* node = nullptr;
        QString stationName;
        QVector3D position;
        QString displayName;
        QString statsText;

        bool operator==(const Entrance&) const = default;
    };

    QPointer<cwCavingRegion> m_region;
    QList<Entrance> m_entrances; //!< row-aligned with markers()
    QSet<cwSurveyNode*> m_connectedNodes;
    bool m_rebuildPending = false;

    void scheduleRebuild();
    void connectRegion();
    void disconnectRegion();
    void syncNodeConnections();
    void disconnectNode(cwSurveyNode* node);
    QList<Entrance> desiredEntrances() const;
    static cwMarker toMarker(const Entrance& entrance);
    static QList<cwMarker> toMarkers(const QList<Entrance>& entrances);

private slots:
    void rebuild();
    void nodeDestroyed(QObject* node);
};

#endif // CWENTRANCEMARKERSOURCE_H
