/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwUnits.h"

//Std includes
#include <algorithm>
#include <limits>

namespace {

using Leg = cwPassageTubeBuilder::Leg;
using Chain = cwPassageTubeBuilder::Chain;
using ChainLeg = cwPassageTubeBuilder::ChainLeg;

std::optional<double> readingToMeters(const cwDistanceReading& reading, cwUnits::LengthUnit unit)
{
    if (reading.state() == cwDistanceReading::State::Valid) {
        return cwUnits::convert(reading.toDouble(), unit, cwUnits::Meters);
    }
    return std::nullopt;
}

cwStationLrud stationLrud(const cwStation& station, cwUnits::LengthUnit unit)
{
    cwStationLrud lrud {
        readingToMeters(station.left(), unit),
        readingToMeters(station.right(), unit),
        readingToMeters(station.up(), unit),
        readingToMeters(station.down(), unit)
    };

    const auto isMeasured = [](const std::optional<double>& value) {
        return value.has_value() && *value != 0.0;
    };

    if (isMeasured(lrud.left) || isMeasured(lrud.right)
        || isMeasured(lrud.up) || isMeasured(lrud.down)) {
        for (std::optional<double>* value : {&lrud.left, &lrud.right, &lrud.up, &lrud.down}) {
            if (value->has_value() && **value == 0.0) {
                *value = cwPassageTubeBuilder::kMinRadiusMeters;
            }
        }
        return lrud;
    }
    return {};
}

QVector<Leg> extractLegs(const cwCavingRegionData& region,
                         const QHash<QUuid, cwStationPositionLookup>& positionsByCave)
{
    QVector<Leg> legs;
    for (const cwCaveData& cave : region.caves) {
        const auto positionsIt = positionsByCave.constFind(cave.id);
        if (positionsIt == positionsByCave.constEnd()) {
            continue;
        }
        const cwStationPositionLookup& positions = positionsIt.value();
        const auto isPositioned = [&](const cwStation& station) {
            return station.isValid() && positions.hasPosition(station.name());
        };

        for (const cwTripData& trip : cave.trips) {
            const cwUnits::LengthUnit unit = trip.calibrations.distanceUnit();
            for (int chunkIndex = 0; chunkIndex < trip.chunks.size(); ++chunkIndex) {
                const cwSurveyChunkData& chunk = trip.chunks.at(chunkIndex);
                const int shotCount = std::min<int>(chunk.shots.size(), chunk.stations.size() - 1);
                for (int shotIndex = 0; shotIndex < shotCount; ++shotIndex) {
                    const cwStation& from = chunk.stations.at(shotIndex);
                    const cwStation& to = chunk.stations.at(shotIndex + 1);
                    if (!isPositioned(from) || !isPositioned(to)) {
                        continue;
                    }

                    const QVector3D fromPosition = positions.position(from.name());
                    const QVector3D toPosition = positions.position(to.name());
                    if (fromPosition.distanceToPoint(toPosition) < cwPassageTubeBuilder::kMinLegLength) {
                        continue;
                    }

                    legs.append(Leg {
                        cave.id,
                        trip.id,
                        chunkIndex,
                        shotIndex,
                        from.name(),
                        to.name(),
                        fromPosition,
                        toPosition,
                        stationLrud(from, unit),
                        stationLrud(to, unit)
                    });
                }
            }
        }
    }
    return legs;
}

QVector<Chain> buildChains(const QVector<Leg>& legs)
{
    // Stations are nodes, keyed per cave: two caves may reuse a station name.
    QHash<std::pair<QUuid, QString>, int> nodeIndexByStation;
    QVector<QVector<int>> legsByNode;
    const auto nodeIndex = [&](const QUuid& caveId, const QString& station) {
        const std::pair<QUuid, QString> key {caveId, cwStation::canonicalKey(station)};
        const auto it = nodeIndexByStation.constFind(key);
        if (it != nodeIndexByStation.constEnd()) {
            return it.value();
        }
        const int index = legsByNode.size();
        nodeIndexByStation.insert(key, index);
        legsByNode.append(QVector<int>());
        return index;
    };

    QVector<int> fromNode(legs.size());
    QVector<int> toNode(legs.size());
    for (int legIndex = 0; legIndex < legs.size(); ++legIndex) {
        const Leg& leg = legs.at(legIndex);
        fromNode[legIndex] = nodeIndex(leg.caveId, leg.from);
        toNode[legIndex] = nodeIndex(leg.caveId, leg.to);
        legsByNode[fromNode.at(legIndex)].append(legIndex);
        legsByNode[toNode.at(legIndex)].append(legIndex);
    }

    QVector<bool> legUsed(legs.size(), false);
    QVector<bool> junctionPassed(legsByNode.size(), false);

    const auto degree = [&](int node) {
        return legsByNode.at(node).size();
    };

    const auto direction = [&](const ChainLeg& chainLeg) {
        const Leg& leg = legs.at(chainLeg.legIndex);
        const QVector3D delta = leg.toPosition - leg.fromPosition;
        return (chainLeg.reversed ? -delta : delta).normalized();
    };

    const auto leaving = [&](int legIndex, int node) {
        return ChainLeg {legIndex, toNode.at(legIndex) == node};
    };

    const auto endNode = [&](const ChainLeg& chainLeg) {
        return chainLeg.reversed ? fromNode.at(chainLeg.legIndex) : toNode.at(chainLeg.legIndex);
    };

    const auto straightestUnusedLeg = [&](int node, const QVector3D& incoming) {
        int bestLeg = -1;
        float bestDot = -std::numeric_limits<float>::max();
        for (int legIndex : legsByNode.at(node)) {
            if (legUsed.at(legIndex)) {
                continue;
            }
            const float dot = QVector3D::dotProduct(incoming, direction(leaving(legIndex, node)));
            if (dot > bestDot) {
                bestDot = dot;
                bestLeg = legIndex;
            }
        }
        return bestLeg;
    };

    const auto walkChain = [&](int startNode, int firstLeg) {
        Chain chain;
        ChainLeg current = leaving(firstLeg, startNode);
        while (true) {
            legUsed[current.legIndex] = true;
            chain.legs.append(current);

            const int node = endNode(current);
            const bool isJunction = degree(node) > 2;
            if (isJunction && junctionPassed.at(node)) {
                break;
            }

            const int nextLeg = straightestUnusedLeg(node, direction(current));
            if (nextLeg < 0) {
                break;
            }
            if (isJunction) {
                junctionPassed[node] = true;
            }
            current = leaving(nextLeg, node);
        }
        return chain;
    };

    // Dead ends first, then odd junctions, then even junctions, so through
    // chains run end to end before arms claim the junctions.
    QVector<int> seeds;
    for (int node = 0; node < legsByNode.size(); ++node) {
        if (degree(node) != 2) {
            seeds.append(node);
        }
    }
    const auto seedRank = [&](int node) {
        if (degree(node) == 1) {
            return 0;
        }
        return degree(node) % 2 == 1 ? 1 : 2;
    };
    std::stable_sort(seeds.begin(), seeds.end(), [&](int a, int b) {
        return seedRank(a) < seedRank(b);
    });
    QVector<Chain> chains;
    for (int node : std::as_const(seeds)) {
        for (int legIndex : legsByNode.at(node)) {
            if (!legUsed.at(legIndex)) {
                chains.append(walkChain(node, legIndex));
            }
        }
    }

    // What remains are loops made only of degree-2 stations.
    for (int legIndex = 0; legIndex < legs.size(); ++legIndex) {
        if (!legUsed.at(legIndex)) {
            chains.append(walkChain(fromNode.at(legIndex), legIndex));
        }
    }

    return chains;
}

} // namespace

cwPassageTubeMesh cwPassageTubeBuilder::build(const cwCavingRegionData& region,
                                              const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                              const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                              const Options& options)
{
    buildDebug(region, positionsByCave, splaysByCave, options);
    return {};
}

cwPassageTubeBuilder::Debug cwPassageTubeBuilder::buildDebug(const cwCavingRegionData& region,
                                                             const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                                             const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                                             const Options& options)
{
    Q_UNUSED(splaysByCave)
    Q_UNUSED(options)

    Debug debug;
    debug.legs = extractLegs(region, positionsByCave);
    debug.chains = buildChains(debug.legs);
    return debug;
}
