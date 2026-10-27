/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLinePlotGeometry.h"
#include "cwCavernNaming.h"
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

// The region's solved positions as one node's trips name them: keyed by the
// canonical name below the node's label path, so each trip resolves the world
// position of any station it references.
QHash<QString, QVector3D> nodeStationPositions(const cwStationPositionLookup& stationPositions,
                                               const QString& nodePrefix)
{
    const QMap<QString, QVector3D> positions = stationPositions.scoped(nodePrefix).positions();
    QHash<QString, QVector3D> out;
    out.reserve(positions.size());
    for (auto iter = positions.constBegin(); iter != positions.constEnd(); ++iter) {
        out.insert(iter.key(), iter.value());
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
// key its node's position lookup and splay tips use ("topo1.a1"). Only the
// prefix is matched ignoring case; the station keeps its solved spelling.
QString nodeLocalKey(const QString& networkKey, const QString& nodePrefix)
{
    if (networkKey.startsWith(nodePrefix, Qt::CaseInsensitive)) {
        return networkKey.sliced(nodePrefix.size());
    }
    return networkKey;
}

// The stations an external-centerline scope (a trip attached to an external
// file) owns in the region-wide network, sorted. `scopePrefix` selects them
// (e.g. "fisher_ridge.topo1."), case-insensitively.
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
                         cwLinePlotGeometry::Result& result)
{
    for (const QVector3D& tip : tips) {
        result.points.append(stationPoint);
        result.points.append(tip);
        result.segmentErrors.append(cwLinePlotGeometry::kNoErrorValue);
    }
}

// The closure error of the leg between two node-local station keys, or
// kNoErrorValue when the leg sits in no loop.
float segmentErrorFor(const cwLegErrorsByShot& legErrors, const QString& from, const QString& to)
{
    const auto it = legErrors.constFind(cwSurvex3DFileReader::legKeyOfKeys(from, to));
    return it == legErrors.constEnd() ? cwLinePlotGeometry::kNoErrorValue : it->e;
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
                                     const cwLegErrorsByShot& legErrors,
                                     cwLinePlotGeometry::Result& result,
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

            result.points.append(from);
            result.points.append(to);
            result.segmentErrors.append(segmentErrorFor(legErrors,
                                                        nodeLocalKey(station, nodePrefix),
                                                        nodeLocalKey(neighbor, nodePrefix)));
        }
    }

    return extent;
}

// Emits one segment per drawn shot of \a trip's chunks, resolving each
// station through \a resolve, and returns their extent. A shot whose station
// has no position is skipped, and the next resolved station bridges back.
template <typename Resolve>
ScopeExtent emitChunkGeometry(const cwTripData& trip, const Resolve& resolve,
                              const cwLegErrorsByShot& legErrors,
                              cwLinePlotGeometry::Result& result)
{
    ScopeExtent extent;
    for (const cwSurveyChunkData& chunk : trip.chunks) {
        if (chunk.stations.size() < 2) {
            continue;
        }

        // Empty leading stations (bug #435) have no position —
        // bootstrap from the first station that has a solved position.
        int startIndex = 0;
        QVector3D previousPoint;
        QString previousName;
        bool havePrevious = false;
        while (startIndex < chunk.stations.size()) {
            previousName = chunk.stations.at(startIndex).name();
            if (resolve(previousName, &previousPoint)) {
                havePrevious = true;
                break;
            }
            startIndex++;
        }
        if (!havePrevious || startIndex >= chunk.stations.size() - 1) {
            continue;
        }

        extent.includeDepth(previousPoint.z());

        for (int stationIndex = startIndex + 1;
             stationIndex < chunk.stations.size();
             stationIndex++) {
            const cwShot& shot = chunk.shots.at(stationIndex - 1);

            QVector3D currentPoint;
            const QString currentName = chunk.stations.at(stationIndex).name();
            if (!resolve(currentName, &currentPoint)) {
                continue;
            }

            if (shot.isDistanceIncluded()) {
                extent.includeDepth(currentPoint.z());
                extent.length += QVector3D(currentPoint - previousPoint).length();
            }

            // Per-shot de-share: emit both endpoints as this shot's own
            // vertices (a station shared with the prior shot is duplicated),
            // so the pair [2i, 2i+1] is one segment.
            result.points.append(previousPoint);
            result.points.append(currentPoint);
            result.segmentErrors.append(segmentErrorFor(legErrors,
                                                        cwStation::canonicalKey(previousName),
                                                        cwStation::canonicalKey(currentName)));

            previousPoint = currentPoint;
            previousName = currentName;
        }
    }
    return extent;
}

// Emits the line segments of \a node's own trips — never its child nodes' —
// and returns their extent.
ScopeExtent emitOwnTrips(const cwCaveData& node,
                         const cwScopeLabels& scopeLabels,
                         const cwStationPositionLookup& regionPositions,
                         const cwSurveyNetwork& network,
                         const QHash<QUuid, cwSplayTipsByStation>& splayTipsByNode,
                         const QHash<QUuid, cwLegErrorsByShot>& legErrorsByNode,
                         cwLinePlotGeometry::Result& result)
{
    // Network keys are region-wide ("fisher_ridge.topo1.<tail>"); the node's
    // positions strip this node's prefix, so external scopes bridge through it.
    const QString nodePrefix = scopeLabels.prefix(node.id);
    const QHash<QString, QVector3D> stationPositions =
        nodeStationPositions(regionPositions, nodePrefix);

    ScopeExtent nodeExtent;

    // Each trip's network-wide scope prefix, empty for a native trip.
    // Gathered before the trip loop so every scope also sees its siblings'
    // and can hand its nested blocks' stations to the deeper scope that
    // owns them.
    QStringList tripScopePrefixes;
    tripScopePrefixes.reserve(node.trips.size());
    for (const cwTripData& trip : node.trips) {
        const QString tripScope = cwTrip::scopePrefix(trip, scopeLabels, node.id);
        tripScopePrefixes.append(tripScope.isEmpty() ? QString() : nodePrefix + tripScope);
    }

    // Undirected de-dup: each in-scope station lists its neighbors, so every
    // leg would otherwise be emitted twice (once from each endpoint) — and a
    // tie leg across a scope boundary once more from the neighboring scope.
    // Shared by the node's scopes, keyed by scope-agnostic network keys.
    QSet<QString> emittedNodeLegs;

    const cwSplayTipsByStation nodeSplayTips = splayTipsByNode.value(node.id);
    const cwLegErrorsByShot nodeLegErrors = legErrorsByNode.value(node.id);
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
        // tripVertexRanges / tripSplayVertexRanges / tripLengths) in walk
        // order, even ones that emit no geometry, so all four tables stay a
        // dense total-trip-count list.
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
            const ScopeExtent scopeExtent = emitNetworkScopeGeometry(
                network, nodePrefix, ownedStations,
                stationPositions, nodeLegErrors, result, emittedNodeLegs);
            result.tripLengths.append(scopeExtent.length);
            nodeExtent.absorb(scopeExtent);

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
                    appendSplaySegments(positionIt.value(), tipsIt.value(), result);
                }
            }
            appendTripRanges(splayStart);
            continue;
        }

        const ScopeExtent chunkExtent = emitChunkGeometry(trip, resolve, nodeLegErrors, result);
        result.tripLengths.append(chunkExtent.length);
        nodeExtent.absorb(chunkExtent);

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
                    appendSplaySegments(stationPoint, nodeSplayTips.value(key), result);
                }
            }
        }
        appendTripRanges(splayStart);
    }

    return nodeExtent;
}

// One drawn leg of a sourced subtree, keyed so a leg two trips draw counts once
// in every node that folds them both.
struct SourcedLeg {
    QString key;
    QVector3D from;
    QVector3D to;
    bool countsLength = true;
    float error = cwLinePlotGeometry::kNoErrorValue;
};

//! A solved position as a hash key: coincident stations share one. Two distinct
//! stations that solve to one point (a zero-length shot) therefore count as one
//! member of each other's units.
QString positionKey(const QVector3D& position)
{
    constexpr int kFloatDigits = 9;
    return QStringLiteral("%1,%2,%3")
        .arg(position.x(), 0, 'g', kFloatDigits)
        .arg(position.y(), 0, 'g', kFloatDigits)
        .arg(position.z(), 0, 'g', kFloatDigits);
}

QString undirectedLegKey(const QString& first, const QString& second)
{
    return first < second ? first + QLatin1Char('\n') + second
                          : second + QLatin1Char('\n') + first;
}

// What a sourced subtree's trips draw, read off the root's view: each trip's
// legs and the root-view stations whose splay tips it carries.
struct SourcedTripGeometry {
    QList<SourcedLeg> legs;
    QStringList splayStations;
};

class SourcedSubtreeGeometry
{
public:
    SourcedSubtreeGeometry(const cwCaveData& root,
                           const cwScopeLabels& scopeLabels,
                           const cwStationPositionLookup& regionPositions,
                           const cwSurveyNetwork& network,
                           const cwSplayTipsByStation& splayTips,
                           const cwLegErrorsByShot& legErrors) :
        m_labels(scopeLabels),
        m_splayTips(splayTips),
        m_legErrors(legErrors)
    {
        // The root's view, in solved keys below the root's label path, as
        // every member set names it.
        const QString rootPrefix = scopeLabels.prefix(root.id);
        if (!rootPrefix.isEmpty()) {
            m_positions = nodeStationPositions(regionPositions, rootPrefix);
            const QString rootKey = cwStation::canonicalKey(rootPrefix);
            QStringList stations = network.stations();
            std::sort(stations.begin(), stations.end());
            for (const QString& station : std::as_const(stations)) {
                if (!station.startsWith(rootKey)) {
                    continue;
                }
                const QString local = station.sliced(rootKey.size());
                QStringList& neighbors = m_neighbors[local];
                const QStringList networkNeighbors = network.neighbors(station);
                for (const QString& neighbor : networkNeighbors) {
                    if (neighbor.startsWith(rootKey)) {
                        neighbors.append(neighbor.sliced(rootKey.size()));
                    }
                }
                std::sort(neighbors.begin(), neighbors.end());
                m_stations.append(local);
                const auto position = m_positions.constFind(local);
                if (position != m_positions.constEnd()) {
                    const QString positionKeyOfStation = positionKey(position.value());
                    m_positionKeys.insert(local, positionKeyOfStation);
                    m_stationsAtPosition[positionKeyOfStation].append(local);
                }
            }
        }

        const QHash<QUuid, QString>& nativeLabels = scopeLabels.tripLabels(root.id);
        for (const QString& label : nativeLabels) {
            m_nativeScopes.append(cwCavernNaming::scopePrefix(label));
        }

        measure(root);

        // The whole-node window draws what no unit drew, leaving the native
        // trips' legs to them.
        if (m_windowTripId.isNull()) {
            return;
        }
        SourcedTripGeometry& window = m_trips[m_windowTripId];
        for (const QString& station : std::as_const(m_stations)) {
            if (isNativeScoped(station)) {
                continue;
            }
            bool stationDraws = m_windowStations.contains(station);
            for (const QString& neighbor : m_neighbors.value(station)) {
                const QString key = undirectedLegKey(station, neighbor);
                if (neighbor <= station || m_drawnByUnits.contains(key)
                    || isNativeScoped(neighbor)) {
                    continue;
                }
                if (appendLeg(station, neighbor, window.legs)) {
                    stationDraws = true;
                }
            }
            if (stationDraws) {
                window.splayStations.append(station);
            }
        }
    }

    const SourcedTripGeometry& trip(const QUuid& tripId) const
    {
        static const SourcedTripGeometry kNone;
        const auto it = m_trips.constFind(tripId);
        return it == m_trips.constEnd() ? kNone : it.value();
    }

    const QHash<QString, QVector3D>& positions() const { return m_positions; }
    const cwSplayTipsByStation& splayTips() const { return m_splayTips; }

private:
    const cwScopeLabels& m_labels;
    const cwSplayTipsByStation m_splayTips;
    const cwLegErrorsByShot m_legErrors;
    QHash<QString, QVector3D> m_positions;
    QHash<QString, QStringList> m_neighbors;
    QStringList m_stations;
    // Each network station's position as a key, and the network stations at
    // each position.
    QHash<QString, QString> m_positionKeys;
    QHash<QString, QStringList> m_stationsAtPosition;
    QStringList m_nativeScopes;
    QHash<QUuid, SourcedTripGeometry> m_trips;
    QSet<QString> m_drawnByUnits;
    QUuid m_windowTripId;
    QSet<QString> m_windowStations;

    bool isNativeScoped(const QString& station) const
    {
        return std::any_of(m_nativeScopes.cbegin(), m_nativeScopes.cend(),
                           [&station](const QString& scope) { return station.startsWith(scope); });
    }

    bool appendLeg(const QString& from, const QString& to, QList<SourcedLeg>& legs) const
    {
        const auto fromIt = m_positions.constFind(from);
        const auto toIt = m_positions.constFind(to);
        if (fromIt == m_positions.constEnd() || toIt == m_positions.constEnd()) {
            return false;
        }
        legs.append({undirectedLegKey(from, to), fromIt.value(), toIt.value(), true,
                     segmentErrorFor(m_legErrors, from, to)});
        return true;
    }

    void measure(const cwCaveData& node)
    {
        const QString cavernPath = cwStation::canonicalKey(m_labels.cavernPath(node.id));
        for (const cwTripData& trip : node.trips) {
            SourcedTripGeometry& geometry = m_trips[trip.id];

            if (!trip.sourcePath.isEmpty()) {
                const QString tripPath =
                    cavernPath + cwStation::canonicalKey(cwTrip::scopePrefix(trip, m_labels, node.id));
                QSet<QString> members;
                for (const QString& station : trip.externalStations) {
                    const QString spelled = tripPath + station;
                    members.insert(m_positions.contains(spelled)
                                       ? spelled
                                       : cwStation::canonicalKey(spelled));
                }
                if (cwTrip::windowsWholeNode(trip, node)) {
                    m_windowTripId = trip.id;
                    m_windowStations = members;
                    continue;
                }

                // Two-endpoint membership: a shared station's leg draws in
                // every unit that holds both of its ends. An equated station
                // ("bravo.A5" tied to "alpha.A5") reaches the network under a
                // different name in each survey's legs; matching by position
                // holds both.
                QSet<QString> memberPositions;
                QStringList sortedMembers;
                for (const QString& member : std::as_const(members)) {
                    const auto position = m_positions.constFind(member);
                    if (position != m_positions.constEnd()) {
                        memberPositions.insert(positionKey(position.value()));
                        sortedMembers.append(member);
                    }
                }
                std::sort(sortedMembers.begin(), sortedMembers.end());
                const auto isMember = [&](const QString& station) {
                    return memberPositions.contains(m_positionKeys.value(station));
                };
                QStringList networkMembers;
                for (const QString& position : std::as_const(memberPositions)) {
                    networkMembers.append(m_stationsAtPosition.value(position));
                }
                std::sort(networkMembers.begin(), networkMembers.end());
                for (const QString& station : std::as_const(networkMembers)) {
                    for (const QString& neighbor : m_neighbors.value(station)) {
                        if (station < neighbor && isMember(neighbor)
                            && appendLeg(station, neighbor, geometry.legs)) {
                            m_drawnByUnits.insert(geometry.legs.constLast().key);
                        }
                    }
                }
                geometry.splayStations = sortedMembers;
                continue;
            }

            const QString tripScope =
                cwStation::canonicalKey(cwTrip::scopePrefix(trip, m_labels, node.id));
            if (!trip.externalCenterline.isEmpty() || tripScope.isEmpty()) {
                continue;
            }
            for (const cwSurveyChunkData& chunk : trip.chunks) {
                QString previous;
                for (int i = 0; i < chunk.stations.size(); i++) {
                    const QString key = tripScope + cwStation::canonicalKey(chunk.stations.at(i).name());
                    if (!m_positions.contains(key)) {
                        continue;
                    }
                    if (!previous.isEmpty() && appendLeg(previous, key, geometry.legs)) {
                        geometry.legs.last().countsLength =
                            chunk.shots.at(i - 1).isDistanceIncluded();
                    }
                    previous = key;
                    geometry.splayStations.append(key);
                }
            }
        }
        for (const cwCaveData& child : node.nodes) {
            measure(child);
        }
    }
};

// Folds \a legs into \a extent, counting a leg another trip of the same fold
// already drew once.
void foldLegs(const QList<SourcedLeg>& legs, QSet<QString>& folded, ScopeExtent& extent)
{
    for (const SourcedLeg& leg : legs) {
        if (folded.contains(leg.key)) {
            continue;
        }
        folded.insert(leg.key);
        extent.includeDepth(leg.from.z());
        extent.includeDepth(leg.to.z());
        if (leg.countsLength) {
            extent.length += QVector3D(leg.to - leg.from).length();
        }
    }
}

// Emits a sourced subtree's trips in walk order — each node's trips, then its
// children — and records every node's length and depth from the legs its
// subtree draws, each leg once. Appends each trip's legs to \a subtreeLegs and
// returns \a node's subtree extent.
ScopeExtent emitSourcedNode(const cwCaveData& node,
                            const SourcedSubtreeGeometry& geometry,
                            QSet<QString>& splaysEmitted,
                            QList<QList<SourcedLeg>>& subtreeLegs,
                            cwLinePlotGeometry::Result& result)
{
    const int firstLegList = subtreeLegs.size();
    for (const cwTripData& trip : node.trips) {
        const SourcedTripGeometry& tripGeometry = geometry.trip(trip.id);
        result.tripUuids.append(trip.id);
        const int vertexStart = result.points.size();
        for (const SourcedLeg& leg : tripGeometry.legs) {
            result.points.append(leg.from);
            result.points.append(leg.to);
            result.segmentErrors.append(leg.error);
        }

        const int splayStart = result.points.size();
        for (const QString& station : tripGeometry.splayStations) {
            const auto tips = geometry.splayTips().constFind(station);
            if (tips == geometry.splayTips().constEnd() || splaysEmitted.contains(station)) {
                continue;
            }
            splaysEmitted.insert(station);
            appendSplaySegments(geometry.positions().value(station), tips.value(), result);
        }

        result.tripSplayVertexRanges.append(
            cwLinePlotGeometry::VertexRange{splayStart, int(result.points.size()) - splayStart});
        result.tripVertexRanges.append(
            cwLinePlotGeometry::VertexRange{vertexStart, int(result.points.size()) - vertexStart});
        subtreeLegs.append(tripGeometry.legs);

        ScopeExtent tripExtent;
        QSet<QString> tripFolded;
        foldLegs(tripGeometry.legs, tripFolded, tripExtent);
        result.tripLengths.append(tripExtent.length);
    }

    for (const cwCaveData& child : node.nodes) {
        emitSourcedNode(child, geometry, splaysEmitted, subtreeLegs, result);
    }

    ScopeExtent extent;
    QSet<QString> folded;
    for (int i = firstLegList; i < subtreeLegs.size(); i++) {
        foldLegs(subtreeLegs.at(i), folded, extent);
    }
    result.nodeLengthAndDepths.insert(
        node.id,
        extent.hasDepth
            ? cwLinePlotGeometry::LengthAndDepth(extent.length, extent.maxDepth - extent.minDepth)
            : cwLinePlotGeometry::LengthAndDepth(0.0, 0.0));
    return extent;
}

// Emits \a node's trips, then each child's subtree, records the node's folded
// length and depth, and returns that subtree extent for the parent to fold.
ScopeExtent emitSubtree(const cwCaveData& node,
                        const cwScopeLabels& scopeLabels,
                        const cwStationPositionLookup& regionPositions,
                        const cwSurveyNetwork& network,
                        const QHash<QUuid, cwSplayTipsByStation>& splayTipsByNode,
                        const QHash<QUuid, cwLegErrorsByShot>& legErrorsByNode,
                        cwLinePlotGeometry::Result& result)
{
    if (isSourceRoot(node) && !node.externalCenterline.isEmpty()) {
        const SourcedSubtreeGeometry geometry(node, scopeLabels, regionPositions, network,
                                              splayTipsByNode.value(node.id),
                                              legErrorsByNode.value(node.id));
        QSet<QString> splaysEmitted;
        QList<QList<SourcedLeg>> subtreeLegs;
        return emitSourcedNode(node, geometry, splaysEmitted, subtreeLegs, result);
    }

    ScopeExtent subtreeExtent =
        emitOwnTrips(node, scopeLabels, regionPositions, network, splayTipsByNode,
                     legErrorsByNode, result);
    for (const cwCaveData& child : node.nodes) {
        subtreeExtent.absorb(
            emitSubtree(child, scopeLabels, regionPositions, network, splayTipsByNode,
                        legErrorsByNode, result));
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
                             const cwStationPositionLookup& stationPositions,
                             const cwSurveyNetwork& network,
                             const QHash<QUuid, cwSplayTipsByStation>& splayTipsByNode,
                             const QHash<QUuid, cwLegErrorsByShot>& legErrorsByNode)
{
    Result result;

    // The same labels the exporter wrote, rebuilt from the same ordered
    // snapshot rather than carried across the boundary (see cwCavernNaming).
    const cwScopeLabels scopeLabels(region);

    for (const cwCaveData& cave : region.caves) {
        emitSubtree(cave, scopeLabels, stationPositions, network, splayTipsByNode,
                    legErrorsByNode, result);
    }

    result.points.squeeze();
    result.tripVertexRanges.squeeze();
    result.tripSplayVertexRanges.squeeze();
    result.tripUuids.squeeze();
    result.tripLengths.squeeze();
    result.segmentErrors.squeeze();

    return Monad::Result<Result>(std::move(result));
}
