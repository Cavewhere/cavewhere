/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFloatingSurveyModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwStation.h"
#include "cwTrip.h"

//Std includes
#include <algorithm>
#include <utility>

namespace {

cwFloatingSurveyModel::Trigger triggerOf(cwFindFloatingSurveys::Result::Trigger trigger)
{
    switch (trigger) {
    case cwFindFloatingSurveys::Result::Trigger::UnconnectedChunks:
        return cwFloatingSurveyModel::UnconnectedChunks;
    case cwFindFloatingSurveys::Result::Trigger::ExternalScope:
        return cwFloatingSurveyModel::ExternalScope;
    }
    return cwFloatingSurveyModel::ExternalScope;
}

//! The station names in the trip's own namespace, which is the spelling every
//! trip-level surface shows. An external scope's records are cave-local
//! ("<tripLabel>.a2") while a native chunk's are already bare, so stripping the
//! trip's own prefix is what makes the two triggers name a station alike. A
//! record written before a rename no longer matches the prefix and keeps the
//! spelling the run recorded — stale, but never wrong about which run said it.
QStringList localStations(const cwTrip* trip, const QStringList& stations)
{
    const QString prefix = cwStation::canonicalKey(trip->scopePrefix());
    if (prefix.isEmpty()) {
        return stations;
    }

    QStringList tails;
    tails.reserve(stations.size());
    for (const QString& station : stations) {
        tails.append(station.startsWith(prefix) ? station.mid(prefix.size()) : station);
    }
    return tails;
}

} // namespace

cwFloatingSurveyModel::cwFloatingSurveyModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

int cwFloatingSurveyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwFloatingSurveyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case CaveNameRole:
        return row.caveName;
    case TripNameRole:
        return row.tripName;
    case TriggerRole:
        return QVariant::fromValue(row.trigger);
    case StationsRole:
        return row.stations;
    case CaveIdRole:
        return row.caveId;
    case TripIdRole:
        return row.tripId;
    }
    return QVariant();
}

QHash<int, QByteArray> cwFloatingSurveyModel::roleNames() const
{
    return {
        { CaveNameRole, QByteArrayLiteral("caveName") },
        { TripNameRole, QByteArrayLiteral("tripName") },
        { TriggerRole, QByteArrayLiteral("trigger") },
        { StationsRole, QByteArrayLiteral("stations") },
        { CaveIdRole, QByteArrayLiteral("caveId") },
        { TripIdRole, QByteArrayLiteral("tripId") }
    };
}

bool cwFloatingSurveyModel::isFloating(const QUuid& tripId) const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [&tripId](const Row& row) {
        return row.tripId == tripId;
    });
}

QStringList cwFloatingSurveyModel::floatingStations(const QUuid& tripId) const
{
    const auto found = std::find_if(m_rows.cbegin(), m_rows.cend(), [&tripId](const Row& row) {
        return row.tripId == tripId;
    });
    return found == m_rows.cend() ? QStringList() : found->stations;
}

void cwFloatingSurveyModel::setRegion(cwCavingRegion* region)
{
    if (m_region == region) {
        return;
    }

    //scopeLabelsChanged is the region's aggregate "a cave or trip was named,
    //added or removed" pulse — exactly the events that move a row's text or
    //drop it — so one connection replaces a sweep over every cave and trip.
    //destroyed covers the one way every row's subject leaves at once, which
    //that pulse cannot announce.
    if (!m_region.isNull()) {
        disconnect(m_region, &cwCavingRegion::scopeLabelsChanged,
                   this, &cwFloatingSurveyModel::rebuildRows);
        disconnect(m_region, &cwCavingRegion::destroyed,
                   this, &cwFloatingSurveyModel::rebuildRows);
    }
    m_region = region;
    if (!m_region.isNull()) {
        connect(m_region, &cwCavingRegion::scopeLabelsChanged,
                this, &cwFloatingSurveyModel::rebuildRows);
        connect(m_region, &cwCavingRegion::destroyed,
                this, &cwFloatingSurveyModel::rebuildRows);
    }

    rebuildRows();
}

void cwFloatingSurveyModel::setResults(const QList<cwFindFloatingSurveys::Result>& results)
{
    m_results = results;
    rebuildRows();
}

void cwFloatingSurveyModel::rebuildRows()
{
    QList<Row> rows;

    if (!m_region.isNull() && !m_results.isEmpty()) {
        QHash<QUuid, cwCave*> caveById;
        QHash<QUuid, cwTrip*> tripById;
        //Bound to const locals: caves() and trips() return by value, so
        //iterating the temporary directly detaches a list the region still
        //shares.
        const QList<cwCave*> caves = m_region->caves();
        for (cwCave* cave : caves) {
            caveById.insert(cave->id(), cave);
            const QList<cwTrip*> trips = cave->trips();
            for (cwTrip* trip : trips) {
                tripById.insert(trip->id(), trip);
            }
        }

        for (const cwFindFloatingSurveys::Result& result : m_results) {
            cwCave* cave = caveById.value(result.caveId);
            cwTrip* trip = tripById.value(result.tripId);
            if (cave == nullptr || trip == nullptr) {
                continue; //named by a run the region has since moved past
            }

            rows.append(Row{result.caveId,
                            result.tripId,
                            cave->name(),
                            trip->name(),
                            triggerOf(result.trigger),
                            localStations(trip, result.stations)});
        }
    }

    if (rows == m_rows) {
        return;
    }

    //A whole-model reset rather than a row diff: a floating survey is something
    //the user is being asked to fix, so the list is a handful of rows at most
    //and never the steady state.
    const bool countChanges = rows.size() != m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();

    if (countChanges) {
        emit countChanged();
    }
}
