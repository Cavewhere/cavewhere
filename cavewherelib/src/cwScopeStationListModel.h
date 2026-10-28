/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCOPESTATIONLISTMODEL_H
#define CWSCOPESTATIONLISTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

//Std includes
#include <optional>

//Our includes
#include "cwGlobals.h"
#include "cwStationHandle.h"

class cwTrip;
class cwSurveyNode;

/**
 * Read-only list model exposing a trip's stations by their scope-relative
 * name, for autocomplete / reference entry at the note, scrap, and LiDAR
 * sites.
 *
 * Set trip and the model lists that trip's stations, one row each: the name is
 * the scope-relative tail cwTrip::knownStations() yields, and the handle is the
 * identity cwTrip::stationHandle() gives that tail — what an equate stores and
 * the only thing a picker should hand on. The model never composes a qualified
 * name; that is the exporter's job.
 *
 * Rows cover every station the trip is known to have, not only the placed ones:
 * a dropped attachment's stations are named by the scan's harvest long before
 * anything solves them, and a person referencing one from a note needs the name
 * then, not after the tie-in that placed it.
 *
 * A caller supplies only the trip; the model subscribes itself to that trip's
 * knownStationsChanged and re-pulls on it (a full modelReset, no incremental
 * diff). Rows come out in the natural order knownStations() sorts them in
 * (trailing station numbers ascend numerically, so "a2" precedes "a10") so
 * consumers stay deterministic.
 *
 * Set node instead (setting one scope clears the other) and the model lists
 * the names a fix on that node takes, the set
 * cwSurvexExporterCaveTask::AnchorPlan's fix validation accepts: each trip's
 * knownStations() under its scopePrefix() ("trip1.entrance"), in node order,
 * then the node's own attachment's stations unscoped, deduplicated by the
 * station each name resolves to, so both members of a case pair list. A
 * source root lists every native trip of its subtree under the label its
 * block gives it. Stations an attached file fixes itself
 * (cwSurveyNode::fileFixedStations()), each name resolved to its solved key,
 * are left out. A row from the node's own attachment has an invalid handle
 * and no position. Node mode coalesces its
 * many pulses into one queued rebuild per event-loop turn.
 */
class CAVEWHERE_LIB_EXPORT cwScopeStationListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScopeStationListModel)

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)
    Q_PROPERTY(cwSurveyNode* node READ node WRITE setNode NOTIFY nodeChanged)

public:
    enum Roles {
        StationNameRole = Qt::UserRole + 1, //trip mode: the handle's tail; node mode: the node-relative name
        StationHandleRole,                  //cwStationHandle, the station's identity
        PositionRole                        //solved QVector3D, undefined when unplaced
    };
    Q_ENUM(Roles)

    explicit cwScopeStationListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    cwTrip* trip() const { return m_trip; }
    void setTrip(cwTrip* trip);

    cwSurveyNode* node() const { return m_node; }
    void setNode(cwSurveyNode* node);

    //Non-blocking membership check: is displayName one of the in-scope
    //station names? Case-insensitive, canonicalized like the rows.
    //Drives the note-entry advisory ("a2 isn't in this trip") without
    //touching the permissive cwStationValidator.
    Q_INVOKABLE bool containsStation(const QString& displayName) const;

    //In-scope display names whose start matches prefix (case-insensitive),
    //in row order, for the autocomplete dropdown + inline ghost. Node mode
    //also matches the start of any dotted part ("ent" finds "trip1.entrance"),
    //listed after the whole-name matches. An empty prefix returns every
    //in-scope name.
    Q_INVOKABLE QStringList matchingStations(const QString& prefix) const;

signals:
    void tripChanged();
    void nodeChanged();

private slots:
    void scheduleRebuild();
    void rebuildPendingRows();
    void nodeDestroyed();

private:
    struct Row {
        QString name;
        QString key;
        cwStationHandle handle;
        std::optional<QVector3D> position;
    };

    void rebuildRows();
    void appendNodeRows();
    void clearTrip();
    void clearNode();

    QPointer<cwTrip> m_trip;
    QPointer<cwSurveyNode> m_node;
    QList<QMetaObject::Connection> m_nodeConnections;
    QList<QMetaObject::Connection> m_nodeTripConnections;
    bool m_rebuildPending = false;
    QList<Row> m_rows;
};

#endif // CWSCOPESTATIONLISTMODEL_H
