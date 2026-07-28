/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFindFloatingSurveys.h"
#include "cwCavernNaming.h"
#include "cwCaveData.h"
#include "cwCavingRegionData.h"
#include "cwEquate.h"
#include "cwScopeLabels.h"
#include "cwStation.h"
#include "cwStationHandle.h"
#include "cwSurveyNetwork.h"

#include <QHash>
#include <QSet>

namespace {

//! The scope a station lives in, named by a uuid that is unique region-wide: an
//! externally-attached trip's own id for its "*begin" block, and the owning
//! cave's id for everything the exporter emits directly under the cave label —
//! native chunks and a cave-level attachment alike.
using ScopeKey = QUuid;

//! Which cave owns each trip, and which trips open a scope of their own. Built
//! once per call so an equate operand can be resolved to a scope without
//! re-walking the region.
class ScopeIndex
{
public:
    explicit ScopeIndex(const cwCavingRegionData& region)
    {
        for (const cwCaveData& cave : region.caves) {
            for (const cwTripData& trip : cave.trips) {
                m_caveOfTrip.insert(trip.id, cave.id);
                if (!trip.externalCenterline.isEmpty()) {
                    m_externalTrips.insert(trip.id);
                }
            }
        }
    }

    //! The scope \a handle names, or a null uuid when it names nothing this
    //! region holds. A Trip handle on a native trip resolves to its cave's own
    //! scope: the exporter emits native chunks unscoped, so that is where the
    //! station actually lands.
    ScopeKey scopeOf(const cwStationHandle& handle) const
    {
        if (!handle.isValid()) {
            return QUuid();
        }
        switch (handle.scope()) {
        case cwStationHandle::NativeCave:
            return handle.containerId();
        case cwStationHandle::Trip:
            return m_externalTrips.contains(handle.containerId())
                       ? handle.containerId()
                       : m_caveOfTrip.value(handle.containerId());
        }
        return QUuid();
    }

private:
    QHash<QUuid, QUuid> m_caveOfTrip;
    QSet<QUuid> m_externalTrips;
};

//! Union-find over scope keys. Every equate in the region joins the scopes its
//! operands live in, and two scopes are connected when they share a root.
class ScopeTies
{
public:
    ScopeTies(const cwCavingRegionData& region, const ScopeIndex& index)
    {
        const auto tie = [this, &index](const QList<cwEquate>& equates) {
            for (const cwEquate& equate : equates) {
                if (!equate.isValid()) {
                    continue; //cannot tie anything; the exporter drops it too
                }
                //Hub on the first operand that resolves, not on operand zero: a
                //handle naming a container this region no longer holds resolves
                //to nothing, and hubbing on it would silently tie none of the
                //operands that are perfectly good.
                ScopeKey hub;
                for (const cwStationHandle& station : equate.stations()) {
                    const ScopeKey scope = index.scopeOf(station);
                    if (hub.isNull()) {
                        hub = scope;
                    } else {
                        join(hub, scope);
                    }
                }
            }
        };

        for (const cwCaveData& cave : region.caves) {
            tie(cave.equates);
        }
        tie(region.equates);
    }

    bool connected(const ScopeKey& first, const ScopeKey& second) const
    {
        return root(first) == root(second);
    }

private:
    ScopeKey root(ScopeKey key) const
    {
        while (true) {
            const ScopeKey parent = m_parent.value(key, key);
            if (parent == key) {
                return key;
            }
            key = parent;
        }
    }

    void join(const ScopeKey& first, const ScopeKey& second)
    {
        if (first.isNull() || second.isNull()) {
            return;
        }
        const ScopeKey firstRoot = root(first);
        const ScopeKey secondRoot = root(second);
        if (firstRoot != secondRoot) {
            m_parent.insert(firstRoot, secondRoot);
        }
    }

    QHash<ScopeKey, ScopeKey> m_parent;
};

//! Every station the region solved, split by the scope each lives in.
//! Membership is a naming question the fused-endpoint problem in the .3d reader
//! cannot touch — only the adjacency it derives is unreliable, and this uses
//! none of it.
//!
//! Built once for the whole region rather than once per cave: a scope key is
//! unique region-wide, so one pass over the network answers for every cave, and
//! cwSurveyNetwork::stations() materializes a fresh list on each call.
class SolvedScopes
{
public:
    SolvedScopes(const cwCavingRegionData& region,
                 const cwScopeLabels& labels,
                 const cwSurveyNetwork& regionNetwork)
    {
        QHash<QString, QUuid> caveByLabel;
        QHash<QUuid, QHash<QString, QUuid>> tripsByCave;
        for (const cwCaveData& cave : region.caves) {
            const QString caveLabel = cwStation::canonicalKey(labels.caveLabel(cave.id));
            if (caveLabel.isEmpty()) {
                continue; //unlabeled, so nothing cavern echoed back can name it
            }
            caveByLabel.insert(caveLabel, cave.id);

            const QHash<QUuid, QString>& tripLabels = labels.tripLabels(cave.id);
            QHash<QString, QUuid>& tripByLabel = tripsByCave[cave.id];
            for (const cwTripData& trip : cave.trips) {
                if (!trip.externalCenterline.isEmpty()) {
                    tripByLabel.insert(cwStation::canonicalKey(tripLabels.value(trip.id)),
                                       trip.id);
                }
            }
        }

        //cwSurveyNetwork keys are already canonical, so they compare directly
        //against the canonicalized cave and trip labels.
        for (const QString& station : regionNetwork.stations()) {
            const auto caveIt = caveByLabel.constFind(cwCavernNaming::scopeHeadOf(station));
            if (caveIt == caveByLabel.constEnd()) {
                continue; //no cave in this region opened that block
            }
            const QString caveLocalName = cwCavernNaming::removeScopeHead(station);
            const auto tripsIt = tripsByCave.constFind(*caveIt);
            const ScopeKey scope =
                tripsIt->value(cwCavernNaming::scopeHeadOf(caveLocalName), *caveIt);
            m_stations[scope].append(caveLocalName);
        }
    }

    bool solvedAnything(const ScopeKey& scope) const { return m_stations.contains(scope); }

    //! Sorted, because the network hands its stations back in hash order.
    QStringList stations(const ScopeKey& scope) const
    {
        QStringList names = m_stations.value(scope);
        names.sort();
        return names;
    }

private:
    QHash<ScopeKey, QStringList> m_stations;
};

//! The scope the rest of the cave is measured against. Normally the cave's own,
//! which is where the "*fix" the exporter emits lands. A cave with no station of
//! its own was never fixed at all, so the first attachment that solved something
//! stands in — a convention, so that a cave holding one attached centerline and
//! nothing else does not report that centerline as adrift from a cave that is
//! otherwise empty.
ScopeKey anchorScopeOf(const cwCaveData& cave, const SolvedScopes& solved)
{
    if (solved.solvedAnything(cave.id)) {
        return cave.id;
    }
    for (const cwTripData& trip : cave.trips) {
        if (!trip.externalCenterline.isEmpty() && solved.solvedAnything(trip.id)) {
            return trip.id;
        }
    }
    return QUuid();
}

void appendFloatingSurveysOf(const cwCaveData& cave,
                             const SolvedScopes& solved,
                             const ScopeTies& ties,
                             QList<cwFindFloatingSurveys::Result>& results)
{
    using Result = cwFindFloatingSurveys::Result;

    const ScopeKey anchor = anchorScopeOf(cave, solved);
    if (anchor.isNull()) {
        return; //the cave solved nothing at all, so it has no answer to give
    }

    for (const cwTripData& trip : cave.trips) {
        if (trip.externalCenterline.isEmpty() || trip.id == anchor) {
            continue;
        }
        //Solving nothing settles it on its own: cavern drops a survey it cannot
        //reach from a fixed point, so an attachment that produced no station is
        //adrift whatever the equates claim.
        if (solved.solvedAnything(trip.id) && ties.connected(trip.id, anchor)) {
            continue;
        }

        Result result;
        result.caveId = cave.id;
        result.tripId = trip.id;
        result.trigger = Result::Trigger::ExternalScope;
        result.stations = solved.stations(trip.id);
        results.append(result);
    }
}

} // namespace

QList<cwFindFloatingSurveys::Result> cwFindFloatingSurveys::fromUnconnectedChunks(
    const cwCaveData& cave,
    const QList<cwFindUnconnectedSurveyChunks::Result>& unconnected)
{
    //Indexed by trip so the records come out in cave order for free.
    QList<QStringList> stationsByTripIndex(cave.trips.size());

    for (const cwFindUnconnectedSurveyChunks::Result& chunkResult : unconnected) {
        if (chunkResult.TripIndex < 0 || chunkResult.TripIndex >= cave.trips.size()) {
            continue;
        }
        const cwTripData& trip = cave.trips.at(chunkResult.TripIndex);
        if (chunkResult.SurveyChunkIndex < 0 || chunkResult.SurveyChunkIndex >= trip.chunks.size()) {
            continue;
        }

        QStringList& stations = stationsByTripIndex[chunkResult.TripIndex];
        for (const cwStation& station : trip.chunks.at(chunkResult.SurveyChunkIndex).stations) {
            if (!station.isValid()) {
                continue;
            }
            const QString name = cwStation::canonicalKey(station.name());
            if (!stations.contains(name)) {
                stations.append(name);
            }
        }
    }

    QList<Result> results;
    for (int tripIndex = 0; tripIndex < cave.trips.size(); tripIndex++) {
        if (stationsByTripIndex.at(tripIndex).isEmpty()) {
            continue;
        }

        Result result;
        result.caveId = cave.id;
        result.tripId = cave.trips.at(tripIndex).id;
        result.trigger = Result::Trigger::UnconnectedChunks;
        result.stations = stationsByTripIndex.at(tripIndex);
        result.stations.sort();
        results.append(result);
    }
    return results;
}

QList<cwFindFloatingSurveys::Result> cwFindFloatingSurveys::fromExternalScopes(
    const cwCavingRegionData& region,
    const cwSurveyNetwork& regionNetwork,
    const cwScopeLabels& labels)
{
    QList<Result> results;
    if (regionNetwork.isEmpty()) {
        //No run has produced a topology yet, so nothing here is knowable —
        //least of all that every attachment is adrift.
        return results;
    }

    const ScopeTies ties(region, ScopeIndex(region));
    const SolvedScopes solved(region, labels, regionNetwork);

    for (const cwCaveData& cave : region.caves) {
        appendFloatingSurveysOf(cave, solved, ties, results);
    }
    return results;
}
