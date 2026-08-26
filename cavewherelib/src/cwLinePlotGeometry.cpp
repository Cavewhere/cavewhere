/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLinePlotGeometry.h"
#include "cwScopeLabels.h"
#include "cwStation.h"
#include "cwTrip.h"

#include <QHash>
#include <QLineF>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <limits>
#include <utility>

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
// network (e.g. "fisher_ridge.topo1."); coordinates are resolved through the
// cave-local position lookup — network keys carry the "fisher_ridge." cave
// prefix that the lookup strips, so `cavePrefix` bridges the two.
// Both prefix comparisons are case-insensitive: a Scope trip windows by its
// `stationPrefix` as authored (which may carry uppercase), while cavern
// lowercases every label it writes to the .3d, so network keys are lowercase.
// `caveScopePrefixes` holds every scope prefix of the same cave (empty entries
// for its native trips): a nested block's stations also carry the parent's
// prefix, so a scope hands a station claimed by a longer sibling prefix to that
// deeper scope, and each leg gets exactly one owner.
// `emitted` is the cave's undirected leg set, shared across the cave's scopes so
// a tie leg reachable from both sides of a scope boundary is drawn once, by the
// first trip that reaches it.
ScopeExtent emitNetworkScopeGeometry(const cwSurveyNetwork& network,
                                     const QString& cavePrefix,
                                     const QString& scopePrefix,
                                     const QStringList& caveScopePrefixes,
                                     const QHash<QString, QVector3D>& stationPositions,
                                     QVector<QVector3D>& points,
                                     QSet<QString>& emitted)
{
    ScopeExtent extent;

    const auto resolveNetworkStation = [&](const QString& networkKey, QVector3D* out) -> bool {
        QString local = networkKey;
        if (local.startsWith(cavePrefix, Qt::CaseInsensitive)) {
            local = local.sliced(cavePrefix.size());
        }
        const auto it = stationPositions.constFind(cwStation::canonicalKey(local));
        if (it == stationPositions.constEnd()) {
            return false;
        }
        *out = it.value();
        return true;
    };

    // Owned by a deeper scope of this cave, which emits it instead. Strictly
    // longer: two scopes sharing a prefix would otherwise hand every station to
    // each other and draw nothing.
    const auto ownedByInnerScope = [&caveScopePrefixes, &scopePrefix](const QString& station) {
        return std::any_of(caveScopePrefixes.cbegin(), caveScopePrefixes.cend(),
                           [&](const QString& inner) {
                               return inner.size() > scopePrefix.size()
                                   && station.startsWith(inner, Qt::CaseInsensitive);
                           });
    };

    // network.stations() comes from a QHash (per-process-randomised order);
    // sort so the emitted vertex buffer is reproducible run to run, matching
    // the deterministic native chunk path.
    QStringList stations = network.stations();
    std::sort(stations.begin(), stations.end());
    for (const QString& station : stations) {
        if (!station.startsWith(scopePrefix, Qt::CaseInsensitive)) {
            continue;
        }
        if (ownedByInnerScope(station)) {
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

    // The same labels the exporter wrote, rebuilt from the same ordered
    // snapshot rather than carried across the boundary (see cwCavernNaming).
    const cwScopeLabels scopeLabels(region);

    for (int caveIndex = 0; caveIndex < caveCount; caveIndex++) {
        const cwCaveData& cave = region.caves.at(caveIndex);
        const QHash<QString, QVector3D> stationPositions = caveStationPositions(cave);

        // Network keys are region-wide ("fisher_ridge.topo1.<tail>"); the
        // cave-local lookup strips this cave prefix, so external scopes bridge
        // through it.
        const QString cavePrefix = scopeLabels.cavePrefix(cave.id);
        const QHash<QUuid, QString>& tripLabels = scopeLabels.tripLabels(cave.id);

        double minDepth = std::numeric_limits<double>::max();
        double maxDepth = -std::numeric_limits<double>::max();
        double length = 0.0;
        bool hasDepth = false;

        // Each trip's network-wide scope prefix, empty for a native trip.
        // Gathered before the trip loop so every scope also sees its siblings'
        // and can hand its nested blocks' stations to the deeper scope that
        // owns them.
        QStringList tripScopePrefixes;
        tripScopePrefixes.reserve(cave.trips.size());
        for (const cwTripData& trip : cave.trips) {
            const QString tripScope = cwTrip::scopePrefix(trip, tripLabels);
            tripScopePrefixes.append(tripScope.isEmpty() ? QString() : cavePrefix + tripScope);
        }

        // Undirected de-dup: each in-scope station lists its neighbors, so every
        // leg would otherwise be emitted twice (once from each endpoint) — and a
        // tie leg across a scope boundary once more from the neighboring scope.
        // Shared by the cave's scopes, keyed by scope-agnostic network keys.
        QSet<QString> emittedCaveLegs;

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
            const QString& scopePrefix = tripScopePrefixes.at(tripIndex);
            if (!scopePrefix.isEmpty()) {
                const ScopeExtent extent = emitNetworkScopeGeometry(
                    network, cavePrefix, scopePrefix, tripScopePrefixes,
                    stationPositions, result.points, emittedCaveLegs);
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

        // Always a real measurement: a cave that resolved no centerline is
        // length 0 and depth 0, which is what an empty cave measures. The
        // value travels straight to cave->length()/depth(), so an "unset"
        // marker would render as one.
        result.cavesLengthAndDepths[caveIndex] = hasDepth
            ? cwLinePlotGeometry::CaveLengthAndDepth(length, maxDepth - minDepth)
            : cwLinePlotGeometry::CaveLengthAndDepth(0.0, 0.0);
    }

    result.points.squeeze();
    result.tripVertexRanges.squeeze();
    result.tripUuids.squeeze();

    return Monad::Result<Result>(std::move(result));
}
