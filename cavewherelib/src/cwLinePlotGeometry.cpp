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

// A node's station positions keyed by canonical station name, built once per
// node so each of its trips can resolve the world position of any station it
// references.
QHash<QString, QVector3D> nodeStationPositions(const cwCaveData& node)
{
    const QMap<QString, QVector3D> positions = node.stationPositionModel.positions();
    QHash<QString, QVector3D> out;
    out.reserve(positions.size());
    for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
        out.insert(cwStation::canonicalKey(iter.key()), iter.value());
    }
    return out;
}

// Depth/length extent for one emitted scope, folded into its node's totals and
// from there into every ancestor's.
struct ScopeExtent {
    double minDepth = std::numeric_limits<double>::max();
    double maxDepth = -std::numeric_limits<double>::max();
    double length = 0.0;
    bool hasDepth = false;

    void includeDepth(double z)
    {
        minDepth = qMin(minDepth, z);
        maxDepth = qMax(maxDepth, z);
        hasDepth = true;
    }

    void absorb(const ScopeExtent& other)
    {
        length += other.length;
        if (other.hasDepth) {
            includeDepth(other.minDepth);
            includeDepth(other.maxDepth);
        }
    }
};

// A region-wide network key ("fisher_ridge.topo1.a1") as the node-local
// canonical key its node's position lookup and splay tips use ("topo1.a1").
QString nodeLocalKey(const QString& networkKey, const QString& nodePrefix)
{
    if (networkKey.startsWith(nodePrefix, Qt::CaseInsensitive)) {
        return cwStation::canonicalKey(networkKey.sliced(nodePrefix.size()));
    }
    return cwStation::canonicalKey(networkKey);
}

// The stations an external-centerline scope (a trip or node attached to an
// external file) owns in the region-wide network, sorted. `scopePrefix`
// selects them (e.g. "fisher_ridge.topo1."), case-insensitively: a Scope trip
// windows by its `stationPrefix` as authored (which may carry uppercase), while
// cavern's labels agree with that case only by luck — it lowercases the Survex
// names it reads and preserves the case of Compass and Walls ones (measured:
// `cave0.AB1`, `cave0:XY:P1`).
// `nodeScopePrefixes` holds every scope prefix of the same node (empty entries
// for its native trips): a nested block's stations also carry the parent's
// prefix, so a scope hands a station claimed by a longer sibling prefix to that
// deeper scope, and each station gets exactly one owner.
QStringList scopeOwnedStations(const cwSurveyNetwork& network,
                               const QString& scopePrefix,
                               const QStringList& nodeScopePrefixes)
{
    // Strictly longer: two scopes sharing a prefix would otherwise hand every
    // station to each other and draw nothing.
    const auto ownedByInnerScope = [&nodeScopePrefixes, &scopePrefix](const QString& station) {
        return std::any_of(nodeScopePrefixes.cbegin(), nodeScopePrefixes.cend(),
                           [&](const QString& inner) {
                               return inner.size() > scopePrefix.size()
                                   && station.startsWith(inner, Qt::CaseInsensitive);
                           });
    };

    // network.stations() comes from a QHash (per-process-randomized order);
    // sort so the emitted vertex buffer is reproducible run to run, matching
    // the deterministic native chunk path.
    QStringList stations = network.stations();
    std::sort(stations.begin(), stations.end());

    QStringList owned;
    for (const QString& station : std::as_const(stations)) {
        if (station.startsWith(scopePrefix, Qt::CaseInsensitive) && !ownedByInnerScope(station)) {
            owned.append(station);
        }
    }
    return owned;
}

// Which running trip id owns each splay-tipped station's segments. Tips are
// keyed per node-local station name while splays are stored per station
// occurrence, so a tie-in name shared between trips has one merged bucket;
// it goes to the first native trip holding an occurrence with splays, falling
// back to the first native trip holding the station at all. Scoped trips
// (`tripScopePrefixes` non-empty) own no chunks; their stations' tips are
// emitted by the scope itself for any station no native trip claims here.
QHash<QString, int> splayOwnerByStation(const cwCaveData& node,
                                        const cwSplayTipsByStation& tips,
                                        const QStringList& tripScopePrefixes,
                                        int firstRunningTripId)
{
    QHash<QString, int> owner;
    if (tips.isEmpty()) {
        // The common case — a node with no splays skips the station walk.
        return owner;
    }

    QSet<QString> ownerHasSplays;
    for (int tripIndex = 0; tripIndex < node.trips.size(); tripIndex++) {
        if (!tripScopePrefixes.at(tripIndex).isEmpty()) {
            continue;
        }
        const int runningTripId = firstRunningTripId + tripIndex;
        for (const cwSurveyChunkData& chunk : node.trips.at(tripIndex).chunks) {
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
    }
    return owner;
}

// Appends one segment per tip, station -> tip. Splays never touch length or
// depth.
void appendSplaySegments(const QVector3D& stationPoint,
                         const QList<QVector3D>& tips,
                         QVector<QVector3D>& points)
{
    for (const QVector3D& tip : tips) {
        points.append(stationPoint);
        points.append(tip);
    }
}

// Emit line segments for an external-centerline scope, whose shot topology
// lives only in the solved survey network. `ownedStations` comes from
// scopeOwnedStations(); coordinates are resolved through the node-local
// position lookup — network keys carry the node prefix ("fisher_ridge.") that
// the lookup strips, so `nodePrefix` bridges the two.
// `emitted` is the node's undirected leg set, shared across the node's scopes so
// a tie leg reachable from both sides of a scope boundary is drawn once, by the
// first trip that reaches it.
ScopeExtent emitNetworkScopeGeometry(const cwSurveyNetwork& network,
                                     const QString& nodePrefix,
                                     const QStringList& ownedStations,
                                     const QHash<QString, QVector3D>& stationPositions,
                                     QVector<QVector3D>& points,
                                     QSet<QString>& emitted)
{
    ScopeExtent extent;

    const auto resolveNetworkStation = [&](const QString& networkKey, QVector3D* out) -> bool {
        const auto it = stationPositions.constFind(nodeLocalKey(networkKey, nodePrefix));
        if (it == stationPositions.constEnd()) {
            return false;
        }
        *out = it.value();
        return true;
    };

    for (const QString& station : ownedStations) {
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

            extent.includeDepth(from.z());
            extent.includeDepth(to.z());
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

// Emits the line segments of \a node's own trips — never its child nodes' —
// and returns their extent.
ScopeExtent emitOwnTrips(const cwCaveData& node,
                         const cwScopeLabels& scopeLabels,
                         const cwSurveyNetwork& network,
                         const QHash<QUuid, cwSplayTipsByStation>& splayTipsByNode,
                         cwLinePlotGeometry::Result& result)
{
    const QHash<QString, QVector3D> stationPositions = nodeStationPositions(node);

    // Network keys are region-wide ("fisher_ridge.topo1.<tail>"); the node's
    // lookup strips this node's prefix, so external scopes bridge through it.
    const QString nodePrefix = scopeLabels.prefix(node.id);
    const QHash<QUuid, QString>& tripLabels = scopeLabels.tripLabels(node.id);

    ScopeExtent nodeExtent;

    // Each trip's network-wide scope prefix, empty for a native trip.
    // Gathered before the trip loop so every scope also sees its siblings'
    // and can hand its nested blocks' stations to the deeper scope that
    // owns them.
    QStringList tripScopePrefixes;
    tripScopePrefixes.reserve(node.trips.size());
    for (const cwTripData& trip : node.trips) {
        if (cwTrip::windowsWholeCave(trip, node)) {
            // The whole-cave window's scope is the node itself, so it windows
            // on the node prefix alone. ownedByInnerScope then hands each of the
            // node's blocks to its own window: every sibling prefix is strictly
            // longer than this one.
            tripScopePrefixes.append(nodePrefix);
            continue;
        }
        const QString tripScope = cwTrip::scopePrefix(trip, tripLabels);
        tripScopePrefixes.append(tripScope.isEmpty() ? QString() : nodePrefix + tripScope);
    }

    // Undirected de-dup: each in-scope station lists its neighbors, so every
    // leg would otherwise be emitted twice (once from each endpoint) — and a
    // tie leg across a scope boundary once more from the neighboring scope.
    // Shared by the node's scopes, keyed by scope-agnostic network keys.
    QSet<QString> emittedNodeLegs;

    const cwSplayTipsByStation nodeSplayTips = splayTipsByNode.value(node.id);
    const QHash<QString, int> splayOwners =
        splayOwnerByStation(node, nodeSplayTips, tripScopePrefixes, result.tripUuids.size());
    QSet<QString> splaysEmitted;

    const auto resolve = [&stationPositions](const QString& stationName, QVector3D* out) -> bool {
        auto posIt = stationPositions.constFind(cwStation::canonicalKey(stationName));
        if (posIt == stationPositions.constEnd()) {
            return false;
        }
        *out = posIt.value();
        return true;
    };

    for (int tripIndex = 0; tripIndex < node.trips.size(); tripIndex++) {
        const cwTripData& trip = node.trips.at(tripIndex);

        // Every trip gets a running id (== index into tripUuids /
        // tripVertexRanges / tripSplayVertexRanges) in walk order, even ones
        // that emit no geometry, so all three tables stay a dense
        // total-trip-count list.
        const int runningTripId = result.tripUuids.size();
        result.tripUuids.append(trip.id);
        const int vertexStart = result.points.size();

        // Splay segments ride at the tail of the trip's span, so the full trip
        // range covers them and the splay sub-range can be masked on its own.
        const auto appendTripRanges = [&](int splayStart) {
            result.tripSplayVertexRanges.append(
                cwLinePlotGeometry::VertexRange{splayStart, int(result.points.size()) - splayStart});
            const int vertexCount = result.points.size() - vertexStart;
            result.tripVertexRanges.append(cwLinePlotGeometry::VertexRange{vertexStart, vertexCount});
        };

        // A scoped trip (externally-attached) has no chunk topology of its
        // own; its shots live only in the solved network. Emit those segments
        // and skip the chunk walk entirely.
        const QString& scopePrefix = tripScopePrefixes.at(tripIndex);
        if (!scopePrefix.isEmpty()) {
            const QStringList ownedStations =
                scopeOwnedStations(network, scopePrefix, tripScopePrefixes);
            nodeExtent.absorb(emitNetworkScopeGeometry(
                network, nodePrefix, ownedStations,
                stationPositions, result.points, emittedNodeLegs));

            const int splayStart = result.points.size();
            if (!nodeSplayTips.isEmpty()) {
                for (const QString& station : ownedStations) {
                    const QString key = nodeLocalKey(station, nodePrefix);
                    if (splayOwners.contains(key) || splaysEmitted.contains(key)) {
                        continue;
                    }
                    const auto tipsIt = nodeSplayTips.constFind(key);
                    const auto positionIt = stationPositions.constFind(key);
                    if (tipsIt == nodeSplayTips.constEnd() || positionIt == stationPositions.constEnd()) {
                        continue;
                    }
                    splaysEmitted.insert(key);
                    appendSplaySegments(positionIt.value(), tipsIt.value(), result.points);
                }
            }
            appendTripRanges(splayStart);
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

            nodeExtent.includeDepth(previousPoint.z());

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
                    nodeExtent.includeDepth(currentPoint.z());
                    nodeExtent.length += QVector3D(currentPoint - previousPoint).length();
                }

                // Per-shot de-share: emit both endpoints as this shot's own
                // vertices (a station shared with the prior shot is
                // duplicated), so the pair [2i, 2i+1] is one segment.
                result.points.append(previousPoint);
                result.points.append(currentPoint);

                previousPoint = currentPoint;
            }
        }

        // Walk the trip's stations in survey order so the tips land
        // deterministically.
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
                    appendSplaySegments(stationPoint, nodeSplayTips.value(key), result.points);
                }
            }
        }
        appendTripRanges(splayStart);
    }

    return nodeExtent;
}

// Emits \a node's trips, then each child's subtree, records the node's folded
// length and depth, and returns that subtree extent for the parent to fold.
ScopeExtent emitSubtree(const cwCaveData& node,
                        const cwScopeLabels& scopeLabels,
                        const cwSurveyNetwork& network,
                        const QHash<QUuid, cwSplayTipsByStation>& splayTipsByNode,
                        cwLinePlotGeometry::Result& result)
{
    ScopeExtent subtreeExtent = emitOwnTrips(node, scopeLabels, network, splayTipsByNode, result);
    for (const cwCaveData& child : node.nodes) {
        subtreeExtent.absorb(emitSubtree(child, scopeLabels, network, splayTipsByNode, result));
    }

    // Always a real measurement: a node that resolved no centerline is length
    // 0 and depth 0, which is what an empty node measures. The value travels
    // straight to the node's length()/depth(), so an "unset" marker would
    // render as one.
    result.nodeLengthAndDepths.insert(
        node.id,
        subtreeExtent.hasDepth
            ? cwLinePlotGeometry::LengthAndDepth(subtreeExtent.length,
                                                 subtreeExtent.maxDepth - subtreeExtent.minDepth)
            : cwLinePlotGeometry::LengthAndDepth(0.0, 0.0));
    return subtreeExtent;
}

} // namespace

Monad::Result<cwLinePlotGeometry::Result>
cwLinePlotGeometry::generate(const cwCavingRegionData& region,
                             const cwSurveyNetwork& network,
                             const QHash<QUuid, cwSplayTipsByStation>& splayTipsByNode)
{
    Result result;

    // The same labels the exporter wrote, rebuilt from the same ordered
    // snapshot rather than carried across the boundary (see cwCavernNaming).
    const cwScopeLabels scopeLabels(region);

    for (const cwCaveData& cave : region.caves) {
        emitSubtree(cave, scopeLabels, network, splayTipsByNode, result);
    }

    result.points.squeeze();
    result.tripVertexRanges.squeeze();
    result.tripSplayVertexRanges.squeeze();
    result.tripUuids.squeeze();

    return Monad::Result<Result>(std::move(result));
}
