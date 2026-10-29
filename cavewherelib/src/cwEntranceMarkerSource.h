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
#include "cwMarkerSource.h"
class cwSurveyNode;

/**
 * @brief The entrance markers: one marker per entrance the last solve flagged
 * (cwCavingRegion::solvedEntrances()), fixed or not, at its solved position.
 *
 * A marker's text is its display name in the "name" style and the
 * cwEntranceLabel length and depth line of its cave under it in the "stats"
 * style. Its cave is the deepest node holding the station when that node lies
 * below the owning node (a Compass .dat or Survex block of an attached file),
 * else the nearest node of kind Cave at or above the owning node, else the
 * owning node. The display name is the label of the owning node's entrance
 * entry for the station when it has one, else the cave's name; an attached
 * entrance has no entry, so it shows the cave's name.
 * Its keyword item extends the owning node's keywords with Type = "Entrance"
 * and Name = the display name. Solve, entrance list, name, kind, size and unit
 * changes coalesce into one queued rebuild that emits at most one signal of
 * each kind.
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

    //! The row of @a node's entrance on @a stationName, matched as station
    //! names match (trimmed, case-insensitive), or -1 when it has no marker.
    Q_INVOKABLE int row(cwSurveyNode* node, const QString& stationName) const;

    QPointF spriteAnchor() const override;
    float depthBias() const override;

signals:
    void regionChanged();

private:
    // What one rebuild pass wants drawn for a solved entrance.
    struct Entrance {
        QUuid id;
        cwSurveyNode* node = nullptr;
        cwSurveyNode* caveNode = nullptr;
        cwSurveyNode* deepestNode = nullptr;
        QString stationName;
        QVector3D position;
        bool fixed = false;
        QString caveName;
        QString label;
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
