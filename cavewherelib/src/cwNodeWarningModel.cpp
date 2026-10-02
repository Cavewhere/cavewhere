/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwNodeWarningModel.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwFixStationModel.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

namespace {

bool isShownWarning(const cwError& error)
{
    return error.type() == cwError::Warning && !error.suppressed();
}

} // namespace

cwNodeWarningModel::cwNodeWarningModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void cwNodeWarningModel::setNode(cwSurveyNode* node)
{
    if (m_node == node) {
        return;
    }
    disconnectAll(m_nodeConnections);
    m_node = node;
    connectNode();
    connectTrips();
    emit nodeChanged();
    rebuild();
}

int cwNodeWarningModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant cwNodeWarningModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const Entry& entry = m_entries.at(index.row());
    switch (role) {
    case MessageRole:
        return entry.message;
    case DetailRole:
        return entry.detail;
    case TargetRole:
        return static_cast<int>(entry.target);
    case TripRole:
        return QVariant::fromValue(entry.trip.data());
    case FixStationRowRole:
        return entry.fixStationRow;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwNodeWarningModel::roleNames() const
{
    return {
        { MessageRole, "message" },
        { DetailRole, "detail" },
        { TargetRole, "target" },
        { TripRole, "trip" },
        { FixStationRowRole, "fixStationRow" }
    };
}

void cwNodeWarningModel::connectNode()
{
    if (m_node == nullptr) {
        return;
    }

    const auto reconnectTripsAndRebuild = [this]() {
        connectTrips();
        rebuild();
    };

    const cwErrorListModel* errors = m_node->errorModel()->errors();
    const cwFixStationModel* fixStations = m_node->fixStations();
    m_nodeConnections = {
        connect(errors, &cwErrorListModel::warningMessagesChanged, this, &cwNodeWarningModel::rebuild),
        connect(errors, &cwErrorListModel::dataChanged, this, &cwNodeWarningModel::rebuild),

        // The node's rows are its direct trips.
        connect(m_node, &cwSurveyNode::rowsInserted, this, reconnectTripsAndRebuild),
        connect(m_node, &cwSurveyNode::rowsRemoved, this, reconnectTripsAndRebuild),
        connect(m_node, &cwSurveyNode::modelReset, this, reconnectTripsAndRebuild),

        // A fix inserted or removed above the warning's fix moves its row.
        connect(fixStations, &cwFixStationModel::rowsInserted, this, &cwNodeWarningModel::rebuild),
        connect(fixStations, &cwFixStationModel::rowsRemoved, this, &cwNodeWarningModel::rebuild),
        connect(fixStations, &cwFixStationModel::rowsMoved, this, &cwNodeWarningModel::rebuild),
        connect(fixStations, &cwFixStationModel::modelReset, this, &cwNodeWarningModel::rebuild),

        // m_node already reads null here, so setNode(nullptr) would return early.
        connect(m_node, &QObject::destroyed, this, [this]() {
            disconnectAll(m_nodeConnections);
            disconnectAll(m_tripConnections);
            emit nodeChanged();
            rebuild();
        })
    };
}

void cwNodeWarningModel::connectTrips()
{
    disconnectAll(m_tripConnections);
    if (m_node == nullptr) {
        return;
    }

    for (const cwTrip* trip : m_node->trips()) {
        const cwErrorListModel* errors = trip->errorModel()->errors();
        m_tripConnections.append(connect(errors, &cwErrorListModel::warningMessagesChanged,
                                         this, &cwNodeWarningModel::rebuild));
        m_tripConnections.append(connect(errors, &cwErrorListModel::dataChanged,
                                         this, &cwNodeWarningModel::rebuild));
    }
}

void cwNodeWarningModel::disconnectAll(QList<QMetaObject::Connection>& connections)
{
    for (const QMetaObject::Connection& connection : std::as_const(connections)) {
        disconnect(connection);
    }
    connections.clear();
}

void cwNodeWarningModel::rebuild()
{
    QList<Entry> entries;
    if (m_node != nullptr) {
        const QList<cwError> nodeErrors = m_node->errorModel()->errors()->toList();
        for (const cwError& error : nodeErrors) {
            if (isShownWarning(error)) {
                entries.append(nodeEntry(error));
            }
        }

        const int unconnectedStations = static_cast<int>(cwErrorTypeId::UnconnectedStations);
        for (cwTrip* trip : m_node->trips()) {
            const QList<cwError> tripErrors = trip->errorModel()->errors()->toList();
            for (const cwError& error : tripErrors) {
                if (isShownWarning(error) && error.errorTypeId() == unconnectedStations) {
                    entries.append(Entry{error.message(), error.detail(), Target::TripPage, trip});
                }
            }
        }
    }

    if (entries == m_entries) {
        return;
    }

    const int oldCount = count();
    beginResetModel();
    m_entries = entries;
    endResetModel();
    if (count() != oldCount) {
        emit countChanged();
    }
}

cwNodeWarningModel::Entry cwNodeWarningModel::nodeEntry(const cwError& error) const
{
    Entry entry{error.message(), error.detail()};
    const QUuid targetId = error.targetId();
    if (targetId.isNull()) {
        return entry;
    }

    if (targetId == m_node->id()) {
        entry.target = Target::SourceLine;
        return entry;
    }

    const int fixStationRow = m_node->fixStations()->indexOf(targetId);
    if (fixStationRow >= 0) {
        entry.target = Target::FixStationRow;
        entry.fixStationRow = fixStationRow;
        return entry;
    }

    for (cwTrip* trip : m_node->trips()) {
        if (trip->id() == targetId) {
            entry.target = Target::TripPage;
            entry.trip = trip;
            return entry;
        }
    }
    return entry;
}
