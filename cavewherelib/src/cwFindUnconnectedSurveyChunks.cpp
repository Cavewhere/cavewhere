/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFindUnconnectedSurveyChunks.h"
#include "cwScopeLabels.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterUtils.h"
#include "cwTrip.h"

#include <QHash>
#include <QSet>
#include <QStringList>

namespace {

//! Union-find over canonical station keys: two keys share a root exactly when
//! cavern would join them.
class StationKeyUnion
{
public:
    QString root(const QString& key) const
    {
        QString current = key;
        QString parent = m_parents.value(current, current);
        while (parent != current) {
            current = parent;
            parent = m_parents.value(current, current);
        }
        return current;
    }

    void join(const QString& first, const QString& second)
    {
        const QString firstRoot = root(first);
        m_parents.insert(root(second), firstRoot);
        // Point both keys straight at the root so later walks stay short.
        m_parents.insert(first, firstRoot);
        m_parents.insert(second, firstRoot);
    }

private:
    QHash<QString, QString> m_parents;
};

//! A native node whose chunks the result reports on, with the cavern keys of
//! each chunk's named stations, indexed [trip][chunk].
struct NativeNode
{
    const cwCaveData* node = nullptr;
    QList<QList<QStringList>> chunkKeys;
};

} // namespace

QHash<QUuid, QList<cwFindUnconnectedSurveyChunks::Result>>
cwFindUnconnectedSurveyChunks::find(const cwCavingRegionData& region,
                                    const QSet<QUuid>& excludedExternalOwners)
{
    const cwScopeLabels labels(region);
    const cwSurvexExporterCaveTask::DriverTree tree(region.caves, labels, excludedExternalOwners);

    StationKeyUnion stations;
    QSet<QString> anchors;
    QList<NativeNode> nativeNodes;

    const auto addSourcedStations = [&](const QString& scope, const QStringList& names) {
        const QSet<QString> keys = cwSurvexExporterUtils::scopedStationKeys(scope, names);
        anchors.unite(keys);
        return keys;
    };

    // Each chunk's named stations as cavern keys under \a tripScope, every
    // chunk's keys joined into one component.
    const auto joinChunkKeys = [&](const cwTripData& trip, const QString& tripScope) {
        QList<QStringList> tripChunkKeys;
        tripChunkKeys.reserve(trip.chunks.size());
        for (const cwSurveyChunkData& chunk : trip.chunks) {
            QStringList& keys = tripChunkKeys.emplaceBack();
            for (const cwStation& station : chunk.stations) {
                if (station.isValid()) {
                    keys.append(cwStation::canonicalKey(tripScope + station.name()));
                }
            }
            for (const QString& key : std::as_const(keys)) {
                stations.join(keys.first(), key);
            }
        }
        return tripChunkKeys;
    };

    const auto visitNative = [&](const cwCaveData& node) {
        const QString nodePrefix = labels.prefix(node.id);

        NativeNode native;
        native.node = &node;
        native.chunkKeys.reserve(node.trips.size());

        QSet<QString> nodeKeys;
        QString originKey;

        for (const cwTripData& trip : node.trips) {
            QList<QStringList>& tripChunkKeys = native.chunkKeys.emplaceBack();
            const QString tripScope = nodePrefix + cwTrip::scopePrefix(trip, labels, node.id);

            if (!trip.externalCenterline.isEmpty()) {
                if (!excludedExternalOwners.contains(trip.id)) {
                    nodeKeys.unite(addSourcedStations(tripScope, trip.externalStations));
                }
                continue;
            }

            tripChunkKeys = joinChunkKeys(trip, tripScope);
            for (int i = 0; i < tripChunkKeys.size(); ++i) {
                const QStringList& keys = tripChunkKeys.at(i);
                nodeKeys.unite(QSet<QString>(keys.cbegin(), keys.cend()));
                if (originKey.isEmpty() && cwSurveyChunk::isValid(trip.chunks.at(i))
                    && !keys.isEmpty()) {
                    originKey = keys.first();
                }
            }
        }

        // A fix counts only on a station of this node; the driver drops any
        // other, and a node left with none gets the origin fix instead.
        bool fixed = false;
        for (const cwFixStation& fix : node.fixStations) {
            const QString key = cwStation::keyIn(nodePrefix + fix.stationName().trimmed(), nodeKeys);
            if (nodeKeys.contains(key)) {
                anchors.insert(key);
                fixed = true;
            }
        }
        if (!fixed && !originKey.isEmpty()) {
            anchors.insert(originKey);
        }

        nativeNodes.append(native);
    };

    // The native trips anywhere in a sourced subtree, keyed as the root's block
    // writes them: the root's label path, the trip's label, then the station.
    // They take no origin fix of their own, so only a tie to an anchored
    // station or a root fix on one of their stations connects one; a derived
    // trip's stations are the root's file stations, anchored above. Collects
    // the native keys into \a nativeKeys.
    const auto visitSourcedSubtree = [&](const auto& self, const QString& rootPrefix,
                                         const cwCaveData& node,
                                         QSet<QString>& nativeKeys) -> void {
        NativeNode native;
        native.node = &node;
        native.chunkKeys.reserve(node.trips.size());
        for (const cwTripData& trip : node.trips) {
            if (isNativeTrip(trip)) {
                const QList<QStringList> tripChunkKeys =
                    joinChunkKeys(trip, rootPrefix + cwTrip::scopePrefix(trip, labels, node.id));
                for (const QStringList& keys : tripChunkKeys) {
                    nativeKeys.unite(QSet<QString>(keys.cbegin(), keys.cend()));
                }
                native.chunkKeys.append(tripChunkKeys);
            } else {
                native.chunkKeys.emplaceBack();
            }
        }
        nativeNodes.append(native);
        for (const cwCaveData& child : node.nodes) {
            self(self, rootPrefix, child, nativeKeys);
        }
    };

    // A sourced root is its *include and its subtree's native trips: its
    // file's stations anchor, and nothing beneath it is a native block.
    const auto visit = [&](const auto& self, const cwCaveData& node) -> void {
        if (node.externalCenterline.isEmpty()) {
            visitNative(node);
            for (const cwCaveData& child : node.nodes) {
                self(self, child);
            }
        } else if (!excludedExternalOwners.contains(node.id)) {
            const QString rootPrefix = labels.prefix(node.id);
            addSourcedStations(rootPrefix, node.externalStations);
            if (isSourceRoot(node)) {
                QSet<QString> nativeKeys;
                visitSourcedSubtree(visitSourcedSubtree, rootPrefix, node, nativeKeys);
                for (const cwFixStation& fix : node.fixStations) {
                    const QString key =
                        cwStation::keyIn(rootPrefix + fix.stationName().trimmed(), nativeKeys);
                    if (nativeKeys.contains(key)) {
                        anchors.insert(key);
                    }
                }
            }
        }
    };
    for (const cwCaveData& cave : region.caves) {
        visit(visit, cave);
    }

    // Each tie joins its operands as the driver writes them; a tie the driver
    // drops (an operand it cannot render) joins nothing.
    for (const cwEquate& equate : region.equates) {
        if (!equate.isValid()) {
            continue;
        }
        QStringList operands;
        const QList<cwStationHandle> handles = equate.stations();
        for (const cwStationHandle& handle : handles) {
            const QString operand = tree.operand(handle);
            if (operand.isEmpty()) {
                operands.clear();
                break;
            }
            operands.append(cwStation::canonicalKey(operand));
        }
        for (const QString& operand : std::as_const(operands)) {
            stations.join(operands.first(), operand);
        }
    }

    QSet<QString> anchoredRoots;
    anchoredRoots.reserve(anchors.size());
    for (const QString& anchor : std::as_const(anchors)) {
        anchoredRoots.insert(stations.root(anchor));
    }

    cwError error;
    error.setMessage(QStringLiteral("Survey leg isn't connect to the cave"));
    error.setType(cwError::Fatal);

    QHash<QUuid, QList<Result>> results;
    for (const NativeNode& native : std::as_const(nativeNodes)) {
        QList<Result> nodeResults;
        for (int t = 0; t < native.chunkKeys.size(); t++) {
            const QList<QStringList>& tripChunkKeys = native.chunkKeys.at(t);
            const cwTripData& trip = native.node->trips.at(t);
            for (int c = 0; c < tripChunkKeys.size(); c++) {
                const QStringList& keys = tripChunkKeys.at(c);
                const bool connected = !keys.isEmpty()
                                       && anchoredRoots.contains(stations.root(keys.first()));
                if (!connected && !cwSurveyChunk::isStationAndShotsEmpty(trip.chunks.at(c))) {
                    nodeResults.append(Result(t, c, error));
                }
            }
        }
        if (!nodeResults.isEmpty()) {
            results.insert(native.node->id, nodeResults);
        }
    }

    return results;
}
