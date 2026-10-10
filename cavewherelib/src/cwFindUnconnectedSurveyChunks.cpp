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

    const auto visitNative = [&](const cwCaveData& node) {
        const QString nodePrefix = labels.prefix(node.id);
        const QHash<QUuid, QString>& tripLabels = labels.tripLabels(node.id);

        NativeNode native;
        native.node = &node;
        native.chunkKeys.reserve(node.trips.size());

        QSet<QString> nodeKeys;
        QString originKey;

        for (const cwTripData& trip : node.trips) {
            QList<QStringList>& tripChunkKeys = native.chunkKeys.emplaceBack();
            const QString tripScope = nodePrefix + cwTrip::scopePrefix(trip, tripLabels);

            if (!trip.externalCenterline.isEmpty()) {
                if (!excludedExternalOwners.contains(trip.id)) {
                    nodeKeys.unite(addSourcedStations(tripScope, trip.externalStations));
                }
                continue;
            }

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
                    nodeKeys.insert(key);
                }
                if (originKey.isEmpty() && cwSurveyChunk::isValid(chunk) && !keys.isEmpty()) {
                    originKey = keys.first();
                }
            }
        }

        // A fix counts only on a station of this node; the driver drops any
        // other, and a node left with none gets the origin fix instead.
        bool fixed = false;
        for (const cwFixStation& fix : node.fixStations) {
            const QString key = cwStation::canonicalKey(nodePrefix + fix.stationName().trimmed());
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

    // A sourced root is its *include alone, so its file's stations anchor and
    // nothing beneath it is a native block.
    const auto visit = [&](const auto& self, const cwCaveData& node) -> void {
        if (node.externalCenterline.isEmpty()) {
            visitNative(node);
            for (const cwCaveData& child : node.nodes) {
                self(self, child);
            }
        } else if (!excludedExternalOwners.contains(node.id)) {
            addSourcedStations(labels.prefix(node.id), node.externalStations);
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
