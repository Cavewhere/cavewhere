/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLinePlotGeometry.h"
#include "cwCavernNaming.h"
#include "cwStation.h"
#include "cwTrip.h"

#include <QHash>
#include <QLineF>
#include <QMap>
#include <QSet>
#include <QString>

#include <algorithm>
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

// Depth/length extent for one emitted scope, folded back into the cave totals
// by the caller.
struct ScopeExtent {
    double minDepth = std::numeric_limits<double>::max();
    double maxDepth = -std::numeric_limits<double>::max();
    double length = 0.0;
    bool hasDepth = false;
};

// Emit line segments for an external-centerline scope (a trip or cave attached
// to an external file), whose shot topology lives only in the solved survey
// network. `scopePrefix` selects the scope's stations from the region-wide
// network (e.g. "cave_<hex>.trip_<hex>."); coordinates are resolved through the
// cave-local position lookup — network keys carry the "cave_<hex>." prefix that
// the lookup strips, so `cavePrefix` bridges the two.
ScopeExtent emitNetworkScopeGeometry(const cwSurveyNetwork& network,
                                     const QString& cavePrefix,
                                     const QString& scopePrefix,
                                     const QHash<QString, QVector3D>& stationPositions,
                                     QVector<QVector3D>& points)
{
    ScopeExtent extent;

    const auto resolveNetworkStation = [&](const QString& networkKey, QVector3D* out) -> bool {
        QString local = networkKey;
        if (local.startsWith(cavePrefix)) {
            local = local.sliced(cavePrefix.size());
        }
        const auto it = stationPositions.constFind(cwStation::canonicalKey(local));
        if (it == stationPositions.constEnd()) {
            return false;
        }
        *out = it.value();
        return true;
    };

    // Undirected de-dup: each in-scope station lists its neighbours, so every
    // internal leg would otherwise be emitted twice (once from each endpoint).
    QSet<QString> emitted;
    // network.stations() comes from a QHash (per-process-randomised order);
    // sort so the emitted vertex buffer is reproducible run to run, matching
    // the deterministic native chunk path.
    QStringList stations = network.stations();
    std::sort(stations.begin(), stations.end());
    for (const QString& station : stations) {
        if (!station.startsWith(scopePrefix)) {
            continue;
        }
        QVector3D from;
        if (!resolveNetworkStation(station, &from)) {
            continue;
        }

        const QStringList neighbors = network.neighbors(station);
        for (const QString& neighbor : neighbors) {
            const QString undirectedKey = (station < neighbor)
                ? station + QLatin1Char('\n') + neighbor
                : neighbor + QLatin1Char('\n') + station;
            if (emitted.contains(undirectedKey)) {
                continue;
            }
            QVector3D to;
            if (!resolveNetworkStation(neighbor, &to)) {
                continue;
            }
            emitted.insert(undirectedKey);

            extent.minDepth = qMin(extent.minDepth, qMin((double)from.z(), (double)to.z()));
            extent.maxDepth = qMax(extent.maxDepth, qMax((double)from.z(), (double)to.z()));
            extent.hasDepth = true;
            // The solved network carries no per-shot distance-included flag, so
            // every leg counts toward length (unlike the native chunk path,
            // which honors shot.isDistanceIncluded()).
            extent.length += QVector3D(to - from).length();

            points.append(from);
            points.append(to);
        }
    }

    return extent;
}

} // namespace

Monad::Result<cwLinePlotGeometry::Result>
cwLinePlotGeometry::generate(const cwCavingRegionData& region,
                             const cwSurveyNetwork& network)
{
    Result result;

    const int caveCount = region.caves.size();
    result.cavesLengthAndDepths.resize(caveCount);

    for (int caveIndex = 0; caveIndex < caveCount; caveIndex++) {
        const cwCaveData& cave = region.caves.at(caveIndex);
        const QHash<QString, QVector3D> stationPositions = caveStationPositions(cave);

        // Network keys are region-wide ("cave_<hex>.trip_<hex>.<tail>"); the
        // cave-local lookup strips this cave prefix, so external scopes bridge
        // through it. cave.name has already been rewritten to cave_<hex> by the
        // worker, so cwCavernNaming::caveScopePrefix(cave.id) reproduces it.
        const QString cavePrefix = cwCavernNaming::caveScopePrefix(cave.id);

        double minDepth = std::numeric_limits<double>::max();
        double maxDepth = -std::numeric_limits<double>::max();
        double length = 0.0;
        bool hasDepth = false;

        for (int tripIndex = 0; tripIndex < cave.trips.size(); tripIndex++) {
            const cwTripData& trip = cave.trips.at(tripIndex);

            // Every trip gets a running id (== index into tripUuids /
            // tripVertexRanges) in iteration order, even ones that emit no
            // geometry, so both tables stay a dense total-trip-count list.
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

            // A scoped trip (externally-attached) has no chunk topology of its
            // own; its shots live only in the solved network. Emit those segments
            // and skip the chunk walk entirely.
            const QString tripScope = cwTrip::scopePrefix(trip);
            if (!tripScope.isEmpty()) {
                const QString scopePrefix = cavePrefix + tripScope;
                const ScopeExtent extent = emitNetworkScopeGeometry(
                    network, cavePrefix, scopePrefix, stationPositions, result.points);
                if (extent.hasDepth) {
                    minDepth = qMin(minDepth, extent.minDepth);
                    maxDepth = qMax(maxDepth, extent.maxDepth);
                    length += extent.length;
                    hasDepth = true;
                }

                const int vertexCount = result.points.size() - vertexStart;
                result.tripVertexRanges.append(VertexRange{vertexStart, vertexCount});
                continue;
            }

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
    result.tripUuids.squeeze();

    return Monad::Result<Result>(std::move(result));
}
