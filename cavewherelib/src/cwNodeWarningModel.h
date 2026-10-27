/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWNODEWARNINGMODEL_H
#define CWNODEWARNINGMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
class cwAttachedCenterlinesModel;
class cwError;
class cwSurveyNode;
class cwTrip;

/**
 * The warnings a survey node's page lists, one row per warning: the node's own
 * unsuppressed warnings (fix stations, attached files, untied stations),
 * followed by the UnconnectedStations warning of each of its direct trips,
 * which the solve writes on the trip rather than on the node.
 *
 * Each row also says where the user fixes it, resolved from the warning's
 * targetId against the node: a row of the node's fix stations, one of its
 * trips' pages, or the node's own attached-file source line. An
 * AttachedFileFixWithoutCS warning opens the Fix Stations page on the first bare
 * fix it names, a read-only row of cwSurveyNode::fixStationTable; an
 * AttachedFileUnfixed or AttachedFileDefaultDatum warning opens the page with
 * no row selected.
 *
 * After the node's own rows come the problems of the nodes below it, one row
 * per kind: how many descendant nodes list that warning on their own page, or
 * hold a survey file that failed to read (from attachedCenterlines and their
 * trips' externalStationsError). Each such row names those nodes and opens the
 * first one's page in tree order, which lists the problem in full. Changes below
 * the node arrive in bursts (one per node the validator or the solve touches),
 * so these rows rebuild once per burst, on the event loop; a change to the
 * node's own rows reuses them.
 *
 * The roll-up covers node-level problems and descendant trips' untied stations.
 * The tree row's badge counts more: every warning below the node, shot data
 * included, which the trip pages list in place.
 */
class CAVEWHERE_LIB_EXPORT cwNodeWarningModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NodeWarningModel)

    Q_PROPERTY(cwSurveyNode* node READ node WRITE setNode NOTIFY nodeChanged)
    Q_PROPERTY(cwAttachedCenterlinesModel* attachedCenterlines READ attachedCenterlines WRITE setAttachedCenterlines NOTIFY attachedCenterlinesChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum class Target : int {
        NoTarget,      //!< the warning names nothing the user can open
        FixStationRow, //!< fixStationRow of the node's Fix Stations page (cwFixStationTreeModel's fix rows), or the page itself at -1
        TripPage,      //!< the trip's page, which also shows a trip's attached file
        SourceLine,    //!< the source line of the file attached to the node itself
        NodePage       //!< the page of the descendant node the row names first
    };
    Q_ENUM(Target)

    enum Roles {
        MessageRole = Qt::UserRole + 1,
        DetailRole,
        TargetRole,
        TripRole,
        FixStationRowRole,
        NodeRole
    };
    Q_ENUM(Roles)

    explicit cwNodeWarningModel(QObject* parent = nullptr);

    cwSurveyNode* node() const { return m_node; }
    void setNode(cwSurveyNode* node);

    cwAttachedCenterlinesModel* attachedCenterlines() const { return m_attachedCenterlines; }
    void setAttachedCenterlines(cwAttachedCenterlinesModel* attachedCenterlines);

    int count() const { return static_cast<int>(m_entries.size()); }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void nodeChanged();
    void attachedCenterlinesChanged();
    void countChanged();

private:
    struct Entry {
        QString message;
        QString detail;
        Target target = Target::NoTarget;
        QPointer<cwTrip> trip;
        int fixStationRow = -1;
        QPointer<cwSurveyNode> node;

        bool operator==(const Entry& other) const
        {
            return message == other.message
                   && detail == other.detail
                   && target == other.target
                   && trip.data() == other.trip.data()
                   && fixStationRow == other.fixStationRow
                   && node.data() == other.node.data();
        }
    };

    QPointer<cwSurveyNode> m_node;
    QPointer<cwAttachedCenterlinesModel> m_attachedCenterlines;
    QList<Entry> m_entries;
    QList<Entry> m_rollUpEntries;
    QList<QMetaObject::Connection> m_nodeConnections;
    QList<QMetaObject::Connection> m_tripConnections;
    QList<QMetaObject::Connection> m_descendantTripConnections;
    QList<QMetaObject::Connection> m_attachedCenterlinesConnections;
    bool m_rebuildScheduled = false;

    void connectNode();
    void connectTrips();
    void disconnectAll(QList<QMetaObject::Connection>& connections);
    void scheduleRebuild();
    void rebuildWithRollUps();
    void rebuild();
    Entry nodeEntry(const cwError& error) const;
    QList<Entry> descendantEntries();
    bool hasFileError(const cwSurveyNode* node) const;
};

#endif // CWNODEWARNINGMODEL_H
