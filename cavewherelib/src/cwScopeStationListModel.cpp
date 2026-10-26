/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScopeStationListModel.h"
#include "cwCavernNaming.h"
#include "cwStation.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

//Qt includes
#include <QSet>

//Std includes
#include <algorithm>

namespace {

void disconnectAll(QList<QMetaObject::Connection>& connections)
{
    for (const QMetaObject::Connection& connection : std::as_const(connections)) {
        QObject::disconnect(connection);
    }
    connections.clear();
}

}


cwScopeStationListModel::cwScopeStationListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int cwScopeStationListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwScopeStationListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case StationNameRole:
        return row.name;
    case StationHandleRole:
        return QVariant::fromValue(row.handle);
    case PositionRole:
        return row.position.has_value() ? QVariant::fromValue(*row.position) : QVariant();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwScopeStationListModel::roleNames() const
{
    return {
        { StationNameRole, "stationName" },
        { StationHandleRole, "stationHandle" },
        { PositionRole, "stationPosition" }
    };
}

void cwScopeStationListModel::setTrip(cwTrip* trip)
{
    if (m_trip == trip) {
        return;
    }

    if (trip != nullptr) {
        clearNode();
    }

    if (m_trip != nullptr) {
        disconnect(m_trip, &cwTrip::knownStationsChanged,
                   this, &cwScopeStationListModel::rebuildRows);
    }

    m_trip = trip;

    if (m_trip != nullptr) {
        connect(m_trip, &cwTrip::knownStationsChanged,
                this, &cwScopeStationListModel::rebuildRows);
    }

    emit tripChanged();
    rebuildRows();
}

void cwScopeStationListModel::setNode(cwSurveyNode* node)
{
    if (m_node == node) {
        return;
    }

    if (node != nullptr) {
        clearTrip();
    }

    disconnectAll(m_nodeConnections);
    m_node = node;

    if (m_node != nullptr) {
        const auto schedule = [this](auto signal) {
            m_nodeConnections.append(
                connect(m_node, signal, this, &cwScopeStationListModel::scheduleRebuild));
        };
        //subtreeChanged also covers trips inserted into or removed from a
        //section below a source root.
        schedule(&cwSurveyNode::insertedTrips);
        schedule(&cwSurveyNode::removedTrips);
        schedule(&cwSurveyNode::subtreeChanged);
        schedule(&cwSurveyNode::scopeLabelsChanged);
        schedule(&cwSurveyNode::tripExternalStationsChanged);
        schedule(&cwSurveyNode::externalStationsChanged);
        schedule(&cwSurveyNode::externalCenterlineChanged);
        schedule(&cwSurveyNode::attachedFixesChanged);
        schedule(&cwSurveyNode::sourceChanged);
        m_nodeConnections.append(
            connect(m_node, &QObject::destroyed, this, &cwScopeStationListModel::nodeDestroyed));
    }

    emit nodeChanged();
    rebuildRows();
}

void cwScopeStationListModel::clearTrip()
{
    if (m_trip == nullptr) {
        return;
    }
    disconnect(m_trip, &cwTrip::knownStationsChanged,
               this, &cwScopeStationListModel::rebuildRows);
    m_trip = nullptr;
    emit tripChanged();
}

void cwScopeStationListModel::clearNode()
{
    if (m_node == nullptr) {
        return;
    }
    disconnectAll(m_nodeConnections);
    m_node = nullptr;
    emit nodeChanged();
}

bool cwScopeStationListModel::containsStation(const QString& displayName) const
{
    const QString key = cwStation::canonicalKey(displayName);
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [&key](const Row& row) {
        return row.key == key;
    });
}

QStringList cwScopeStationListModel::matchingStations(const QString& prefix) const
{
    const QString key = cwStation::canonicalKey(prefix);
    QStringList matches;
    QStringList partMatches;
    const bool matchParts = m_node != nullptr && !key.isEmpty();
    const QString partKey = matchParts ? QLatin1Char('.') + key : QString();
    for (const Row& row : m_rows) {
        if (row.key.startsWith(key)) {
            matches.append(row.name);
        } else if (matchParts && row.key.contains(partKey)) {
            partMatches.append(row.name);
        }
    }
    matches.append(partMatches);
    return matches;
}

void cwScopeStationListModel::scheduleRebuild()
{
    if (m_rebuildPending) {
        return;
    }
    m_rebuildPending = true;
    QMetaObject::invokeMethod(this, &cwScopeStationListModel::rebuildPendingRows,
                              Qt::QueuedConnection);
}

void cwScopeStationListModel::nodeDestroyed()
{
    disconnectAll(m_nodeConnections);
    emit nodeChanged();
    scheduleRebuild();
}

void cwScopeStationListModel::rebuildPendingRows()
{
    //Cleared when a synchronous rebuild already ran since the schedule.
    if (m_rebuildPending) {
        rebuildRows();
    }
}

void cwScopeStationListModel::rebuildRows()
{
    m_rebuildPending = false;

    beginResetModel();
    m_rows.clear();
    disconnectAll(m_nodeTripConnections);

    if (m_trip != nullptr) {
        //Already deduplicated and in natural order — cwTrip::knownStations()
        //owns both, so every surface reading a trip's stations lists them the
        //same way.
        const QList<cwTrip::KnownStation> known = m_trip->knownStations();
        m_rows.reserve(known.size());
        for (const cwTrip::KnownStation& station : known) {
            m_rows.append({ station.name, cwStation::canonicalKey(station.name),
                            m_trip->stationHandle(station.name), station.position });
        }
    } else if (m_node != nullptr) {
        appendNodeRows();
    }

    endResetModel();
}

void cwScopeStationListModel::appendNodeRows()
{
    //Restates the accepted-name set of cwSurvexExporterCaveTask::AnchorPlan's
    //fix validation on the live tree, with knownStations() in place of the
    //snapshot's chunks and harvest; keep the two in step.
    const QHash<QString, QString> fileFixed = m_node->fileFixedStations();
    QSet<QString> seen;

    const auto appendRow = [&](const QString& name, const cwStationHandle& handle,
                               const std::optional<QVector3D>& position) {
        const QString key = cwStation::canonicalKey(name);
        if (key.isEmpty() || fileFixed.contains(key) || seen.contains(key)) {
            return;
        }
        seen.insert(key);
        m_rows.append({ name, key, handle, position });
    };

    //The handle cwTrip::stationHandle() gives, from a prefix already in hand:
    //asking the trip would recompute its prefix for every station.
    const auto appendTrip = [&](cwTrip* trip, const QString& prefix) {
        m_nodeTripConnections.append(connect(trip, &cwTrip::knownStationsChanged,
                                             this, &cwScopeStationListModel::scheduleRebuild));
        const bool nativeCave = prefix.isEmpty() && !trip->isDerived();
        const cwSurveyNode* parent = trip->parentNode();
        const cwStationHandle::Scope scope = nativeCave ? cwStationHandle::NativeCave
                                                        : cwStationHandle::Trip;
        const QUuid containerId = nativeCave ? (parent != nullptr ? parent->id() : QUuid())
                                             : trip->id();
        const QList<cwTrip::KnownStation> known = trip->knownStations();
        for (const cwTrip::KnownStation& station : known) {
            appendRow(prefix + station.name, cwStationHandle(scope, containerId, station.name),
                      station.position);
        }
    };

    if (m_node->isSourceRoot()) {
        //Labels taken once: a sourced trip's scopePrefix() walks the whole
        //subtree on each call.
        const QHash<QUuid, QString> labels = m_node->sourcedTripScopeLabels();
        m_node->walk([&](const cwSurveyNode* node) {
            const QList<cwTrip*> trips = node->trips();
            for (cwTrip* trip : trips) {
                if (trip->sourcePath().isEmpty() && trip->externalCenterline().isEmpty()) {
                    appendTrip(trip, cwCavernNaming::scopePrefix(labels.value(trip->id())));
                }
            }
        });
        const QList<cwTrip*> trips = m_node->trips();
        for (cwTrip* trip : trips) {
            if (!trip->externalCenterline().isEmpty()) {
                appendTrip(trip, trip->scopePrefix());
            }
        }
    } else {
        const QList<cwTrip*> trips = m_node->trips();
        for (cwTrip* trip : trips) {
            appendTrip(trip, trip->scopePrefix());
        }
    }

    const QStringList nodeStations = m_node->externalStations();
    for (const QString& station : nodeStations) {
        appendRow(station, cwStationHandle(), std::nullopt);
    }
}
