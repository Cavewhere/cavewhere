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
 * AttachedFixWithoutCS warning opens the Fix Stations page on the first bare
 * fix it names, a read-only row of cwSurveyNode::fixStationTable; an
 * AttachedFileUnfixed warning opens the page with no row selected.
 */
class CAVEWHERE_LIB_EXPORT cwNodeWarningModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NodeWarningModel)

    Q_PROPERTY(cwSurveyNode* node READ node WRITE setNode NOTIFY nodeChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum class Target : int {
        NoTarget,      //!< the warning names nothing the user can open
        FixStationRow, //!< fixStationRow of the node's fixStationTable, or the page itself at -1
        TripPage,      //!< the trip's page, which also shows a trip's attached file
        SourceLine     //!< the source line of the file attached to the node itself
    };
    Q_ENUM(Target)

    enum Roles {
        MessageRole = Qt::UserRole + 1,
        DetailRole,
        TargetRole,
        TripRole,
        FixStationRowRole
    };
    Q_ENUM(Roles)

    explicit cwNodeWarningModel(QObject* parent = nullptr);

    cwSurveyNode* node() const { return m_node; }
    void setNode(cwSurveyNode* node);

    int count() const { return static_cast<int>(m_entries.size()); }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void nodeChanged();
    void countChanged();

private:
    struct Entry {
        QString message;
        QString detail;
        Target target = Target::NoTarget;
        QPointer<cwTrip> trip;
        int fixStationRow = -1;

        bool operator==(const Entry& other) const
        {
            return message == other.message
                   && detail == other.detail
                   && target == other.target
                   && trip.data() == other.trip.data()
                   && fixStationRow == other.fixStationRow;
        }
    };

    QPointer<cwSurveyNode> m_node;
    QList<Entry> m_entries;
    QList<QMetaObject::Connection> m_nodeConnections;
    QList<QMetaObject::Connection> m_tripConnections;

    void connectNode();
    void connectTrips();
    void disconnectAll(QList<QMetaObject::Connection>& connections);
    void rebuild();
    Entry nodeEntry(const cwError& error) const;
};

#endif // CWNODEWARNINGMODEL_H
