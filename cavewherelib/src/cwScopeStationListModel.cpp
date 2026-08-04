/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScopeStationListModel.h"
#include "cwStation.h"
#include "cwTrip.h"

//Std includes
#include <algorithm>


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
        return row.handle.tail();
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

bool cwScopeStationListModel::containsStation(const QString& displayName) const
{
    const QString key = cwStation::canonicalKey(displayName);
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [&key](const Row& row) {
        return cwStation::canonicalKey(row.handle.tail()) == key;
    });
}

QStringList cwScopeStationListModel::matchingStations(const QString& prefix) const
{
    const QString key = cwStation::canonicalKey(prefix);
    QStringList matches;
    for (const Row& row : m_rows) {
        if (cwStation::canonicalKey(row.handle.tail()).startsWith(key)) {
            matches.append(row.handle.tail());
        }
    }
    return matches;
}

void cwScopeStationListModel::rebuildRows()
{
    beginResetModel();
    m_rows.clear();

    if (m_trip != nullptr) {
        //Already deduplicated and in natural order — cwTrip::knownStations()
        //owns both, so every surface reading a trip's stations lists them the
        //same way.
        const QList<cwTrip::KnownStation> known = m_trip->knownStations();
        m_rows.reserve(known.size());
        for (const cwTrip::KnownStation& station : known) {
            m_rows.append({ m_trip->stationHandle(station.name), station.position });
        }
    }

    endResetModel();
}
