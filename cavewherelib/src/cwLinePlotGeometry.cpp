/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLinePlotGeometry.h"
#include "cwStation.h"

#include <QHash>
#include <QLineF>
#include <QMap>
#include <QSet>
#include <QString>

#include <limits>

namespace {

// Cave station positions keyed by canonical station name, built once per cave
// so each trip can resolve the world position of any station it references.
QHash<QString, QVector3D> caveStationPositions(const cwCaveData& cave)
{
    const QMap<QString, QVector3D> positions = cave.stationPositionModel.positions();
    QHash<QString, QVector3D> out;
    out.reserve(positions.size());
    for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
        out.insert(cwStation::canonicalKey(iter.key()), iter.value());
    }
    return out;
}

// Which running trip id owns each splay-tipped station's segments. Tips are
// keyed per cave-level station name while splays are stored per station
// occurrence, so a tie-in name shared between trips has one merged bucket;
// it goes to the first trip holding an occurrence with splays, falling back
// to the first trip holding the station at all (tips from an external .svx
// include match no stored splays).
QHash<QString, int> splayOwnerByStation(const cwCaveData& cave,
                                        const cwSplayTipsByStation& tips,
                                        int firstRunningTripId)
{
    QHash<QString, int> owner;
    if (tips.isEmpty()) {
        // The common case — a cave with no splays skips the station walk.
        return owner;
    }

    QSet<QString> ownerHasSplays;
    int runningTripId = firstRunningTripId;
    for (const cwTripData& trip : cave.trips) {
        for (const cwSurveyChunkData& chunk : trip.chunks) {
            for (const cwStation& station : chunk.stations) {
                const QString key = cwStation::canonicalKey(station.name());
                if (!tips.contains(key)) {
                    continue;
                }
                if (!owner.contains(key)) {
                    owner.insert(key, runningTripId);
                }
                if (station.splayCount() > 0 && !ownerHasSplays.contains(key)) {
                    // The first splays-bearing occurrence wins over the plain
                    // first occurrence.
                    owner.insert(key, runningTripId);
                    ownerHasSplays.insert(key);
                }
            }
        }
        runningTripId++;
    }
    return owner;
}

} // namespace

Monad::Result<cwLinePlotGeometry::Result>
cwLinePlotGeometry::generate(const cwCavingRegionData& region,
                             const QHash<QUuid, cwSplayTipsByStation>& splayTipsByCave)
{
    Result result;

    const int caveCount = region.caves.size();
    result.cavesLengthAndDepths.resize(caveCount);

    for (int caveIndex = 0; caveIndex < caveCount; caveIndex++) {
        const cwCaveData& cave = region.caves.at(caveIndex);
        const QHash<QString, QVector3D> stationPositions = caveStationPositions(cave);

        const cwSplayTipsByStation caveSplayTips = splayTipsByCave.value(cave.id);
        const QHash<QString, int> splayOwners =
            splayOwnerByStation(cave, caveSplayTips, result.tripUuids.size());
        QSet<QString> splaysEmitted;

        double minDepth = std::numeric_limits<double>::max();
        double maxDepth = -std::numeric_limits<double>::max();
        double length = 0.0;
        bool hasDepth = false;

        for (int tripIndex = 0; tripIndex < cave.trips.size(); tripIndex++) {
            const cwTripData& trip = cave.trips.at(tripIndex);

            // Every trip gets a running id (== index into tripUuids /
            // tripVertexRanges / tripSplayVertexRanges) in iteration order,
            // even ones that emit no geometry, so all three tables stay a
            // dense total-trip-count list.
            const int runningTripId = result.tripUuids.size();
            result.tripUuids.append(trip.id);
            const int vertexStart = result.points.size();

            const auto resolve = [&](const QString& stationName, QVector3D* out) -> bool {
                auto posIt = stationPositions.constFind(cwStation::canonicalKey(stationName));
                if (posIt == stationPositions.constEnd()) {
                    return false;
                }
                *out = posIt.value();
                return true;
            };

            for (const cwSurveyChunkData& chunk : trip.chunks) {
                if (chunk.stations.size() < 2) {
                    continue;
                }

                // Empty leading stations (bug #435) have no position —
                // bootstrap from the first station that has a solved position.
                int startIndex = 0;
                QVector3D previousPoint;
                bool havePrevious = false;
                while (startIndex < chunk.stations.size()) {
                    if (resolve(chunk.stations.at(startIndex).name(), &previousPoint)) {
                        havePrevious = true;
                        break;
                    }
                    startIndex++;
                }
                if (!havePrevious || startIndex >= chunk.stations.size() - 1) {
                    continue;
                }

                minDepth = qMin(minDepth, (double)previousPoint.z());
                maxDepth = qMax(maxDepth, (double)previousPoint.z());
                hasDepth = true;

                for (int stationIndex = startIndex + 1;
                     stationIndex < chunk.stations.size();
                     stationIndex++) {
                    const cwShot& shot = chunk.shots.at(stationIndex - 1);

                    QVector3D currentPoint;
                    if (!resolve(chunk.stations.at(stationIndex).name(), &currentPoint)) {
                        // Unresolved station — skip the shot; the next resolved
                        // station bridges back to previousPoint, exactly as the
                        // shared-vertex path did.
                        continue;
                    }

                    if (shot.isDistanceIncluded()) {
                        minDepth = qMin(minDepth, (double)currentPoint.z());
                        maxDepth = qMax(maxDepth, (double)currentPoint.z());
                        length += QVector3D(currentPoint - previousPoint).length();
                    }

                    // Per-shot de-share: emit both endpoints as this shot's own
                    // vertices (a station shared with the prior shot is
                    // duplicated), so the pair [2i, 2i+1] is one segment.
                    result.points.append(previousPoint);
                    result.points.append(currentPoint);

                    previousPoint = currentPoint;
                }
            }

            // Splay segments ride at the tail of the trip's span, so the full
            // trip range covers them and the splay sub-range can be masked on
            // its own. Walk the trip's stations in survey order so the tips
            // land deterministically. Splays never touch length, but their
            // tips extend depth: floor and ceiling splays reach the cave's
            // true vertical extremes, which stations only approximate.
            const int splayStart = result.points.size();
            if (!splayOwners.isEmpty()) {
                for (const cwSurveyChunkData& chunk : trip.chunks) {
                    for (const cwStation& station : chunk.stations) {
                        const QString key = cwStation::canonicalKey(station.name());
                        if (splayOwners.value(key, -1) != runningTripId
                            || splaysEmitted.contains(key)) {
                            continue;
                        }
                        splaysEmitted.insert(key);

                        QVector3D stationPoint;
                        if (!resolve(station.name(), &stationPoint)) {
                            continue;
                        }
                        const QList<QVector3D>& tips = caveSplayTips.value(key);
                        for (const QVector3D& tip : tips) {
                            result.points.append(stationPoint);
                            result.points.append(tip);
                            minDepth = qMin(minDepth, (double)tip.z());
                            maxDepth = qMax(maxDepth, (double)tip.z());
                            hasDepth = true;
                        }
                    }
                }
            }
            result.tripSplayVertexRanges.append(
                VertexRange{splayStart, int(result.points.size()) - splayStart});

            const int vertexCount = result.points.size() - vertexStart;
            result.tripVertexRanges.append(VertexRange{vertexStart, vertexCount});
        }

        if (hasDepth) {
            const double depth = maxDepth - minDepth;
            result.cavesLengthAndDepths[caveIndex] =
                cwLinePlotGeometry::CaveLengthAndDepth(length, depth);
        }
    }

    result.points.squeeze();
    result.tripVertexRanges.squeeze();
    result.tripSplayVertexRanges.squeeze();
    result.tripUuids.squeeze();

    return Monad::Result<Result>(std::move(result));
}
