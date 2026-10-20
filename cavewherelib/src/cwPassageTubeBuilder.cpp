/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeGeometry.h"
#include "cwPassageTubeMeshCheck.h"
#include "cwPassageTubeUnion.h"
#include "cwUnits.h"

//Qt includes
#include <QMap>
#include <QSet>
#include <QtMath>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>

namespace {

using namespace cwPassageTubeGeometry;

// A leg within this angle of plumb reads level sections.
constexpr double kSteepLegDegrees = 45.0;
constexpr double kShaftRatio = 3.0;
constexpr double kMinSampleSpacingMeters = 0.01;
constexpr double kMaxChainTurnDegrees = 100.0;
// An arm this close to a bigger arm at the same junction opens into it,
// which the stitch cannot join, so a through pair that keeps every arm at
// least this far from a bigger arm wins over a straighter one.
constexpr double kMinArmSpreadDegrees = 60.0;
// A through that ends at a junction carries its end ring past it by each
// arm's largest reading over the sine of its angle to the through, floored
// at this angle as the stitch's reach is, plus the margin.
constexpr double kMinThroughReachDegrees = 10.0;
constexpr double kThroughReachMarginMeters = 0.5;
// Two shots between the same stations whose ends lie this close are one
// passage surveyed twice.
constexpr double kRepeatedShotToleranceMeters = 0.1;
// A ring without readings borrows from its own trip within this many legs
// before a nearer ring of another trip.
constexpr int kSameTripBorrowLegs = 2;
constexpr int kMinLoopStations = 3;
constexpr double kOnAxisMeters = 1e-6;
constexpr float kMinHorizontalLength = 0.01f;
constexpr float kDegenerateLengthSquared = 1e-12f;
constexpr QVector3D kWorldUp(0.0f, 0.0f, 1.0f);
constexpr QVector3D kWorldNorth(0.0f, 1.0f, 0.0f);

using Leg = cwPassageTubeBuilder::Leg;
using Chain = cwPassageTubeBuilder::Chain;
using ChainLeg = cwPassageTubeBuilder::ChainLeg;

using Warning = cwPassageTubeMesh::Warning;

std::optional<double> readingToMeters(const cwDistanceReading& reading, cwUnits::LengthUnit unit)
{
    if (reading.state() == cwDistanceReading::State::Valid) {
        return cwUnits::convert(reading.toDouble(), unit, cwUnits::Meters);
    }
    return std::nullopt;
}

cwStationLrud stationLrud(const cwStation& station,
                          cwUnits::LengthUnit unit,
                          const QUuid& tripId,
                          QVector<Warning>& warnings)
{
    cwStationLrud lrud {
        readingToMeters(station.left(), unit),
        readingToMeters(station.right(), unit),
        readingToMeters(station.up(), unit),
        readingToMeters(station.down(), unit)
    };

    const std::initializer_list<std::optional<double>*> values {&lrud.left, &lrud.right, &lrud.up, &lrud.down};
    for (std::optional<double>* value : values) {
        if (value->has_value() && **value > cwPassageTubeBuilder::kMaxRadiusMeters) {
            warnings.append(Warning {
                Warning::LrudOutOfRange,
                tripId,
                station.name(),
                QStringLiteral("Station %1 has a passage dimension of %2 m, above the %3 m limit; the tube ignores it.")
                    .arg(station.name())
                    .arg(**value)
                    .arg(cwPassageTubeBuilder::kMaxRadiusMeters)
            });
            value->reset();
        }
    }

    const auto isMeasured = [](const std::optional<double>& value) {
        return value.has_value() && *value != 0.0;
    };

    if (isMeasured(lrud.left) || isMeasured(lrud.right)
        || isMeasured(lrud.up) || isMeasured(lrud.down)) {
        for (std::optional<double>* value : values) {
            if (value->has_value() && **value == 0.0) {
                *value = cwPassageTubeBuilder::kMinRadiusMeters;
            }
        }
        return lrud;
    }
    return {};
}

bool isPositioned(const cwStation& station, const cwStationPositionLookup& positions)
{
    return station.isValid() && positions.hasPosition(station.name());
}

// Each chunk station's facing: the horizontal chord between its nearest
// positioned neighbors in the chunk. A chunk that closes a loop by ending at
// its first station wraps around, so that station faces along the chord
// between its neighbors on both sides of the loop. A station on a vertical
// chord takes the facing of the nearest station that has one; a chunk
// without any faces north.
QVector<QVector3D> chunkFacings(const cwSurveyChunkData& chunk,
                                const cwStationPositionLookup& positions)
{
    QVector<int> positioned;
    for (int i = 0; i < chunk.stations.size(); ++i) {
        if (isPositioned(chunk.stations.at(i), positions)) {
            positioned.append(i);
        }
    }

    const auto position = [&](int index) {
        return positions.position(chunk.stations.at(index).name());
    };

    const int last = positioned.size() - 1;
    const bool closesLoop = last >= kMinLoopStations
                            && cwStation::canonicalKey(chunk.stations.at(positioned.first()).name())
                                   == cwStation::canonicalKey(chunk.stations.at(positioned.last()).name());

    QVector<std::optional<QVector3D>> chordFacings(chunk.stations.size());
    for (int p = 0; p < positioned.size(); ++p) {
        int previous = positioned.at(std::max(p - 1, 0));
        int next = positioned.at(std::min(p + 1, last));
        if (closesLoop && p == 0) {
            previous = positioned.at(last - 1);
        } else if (closesLoop && p == last) {
            next = positioned.at(1);
        }
        QVector3D chord = position(next) - position(previous);
        chord.setZ(0.0f);
        if (chord.length() >= kMinHorizontalLength) {
            chordFacings[positioned.at(p)] = chord.normalized();
        }
    }

    const int count = chordFacings.size();
    const auto nearestFacing = [&](int index) {
        for (int offset = 0; offset < count; ++offset) {
            for (int candidate : {index - offset, index + offset}) {
                if (candidate >= 0 && candidate < count && chordFacings.at(candidate)) {
                    return *chordFacings.at(candidate);
                }
            }
        }
        return kWorldNorth;
    };

    QVector<QVector3D> facings;
    facings.reserve(count);
    for (int i = 0; i < count; ++i) {
        facings.append(nearestFacing(i));
    }
    return facings;
}

// The mean of two readings of one wall, or the one that was read.
std::optional<double> meanReading(const std::optional<double>& a, const std::optional<double>& b)
{
    if (a && b) {
        return (*a + *b) / 2.0;
    }
    return a ? a : b;
}

cwStationLrud meanLrud(const cwStationLrud& a, const cwStationLrud& b)
{
    return {meanReading(a.left, b.left), meanReading(a.right, b.right), meanReading(a.up, b.up), meanReading(a.down, b.down)};
}

// Legs between the same two stations, surveyed again on a later trip or
// chunk, collapse into the first: it keeps its trip and facing, and each of
// its stations takes the mean of the readings both surveys made there.
QVector<Leg> mergeRepeatedShots(const QVector<Leg>& legs, const QHash<QUuid, QString>& tripNames, QVector<Warning>& warnings)
{
    using StationPair = std::pair<QUuid, std::pair<QString, QString>>;
    QHash<StationPair, int> firstLeg;
    QVector<Leg> merged;
    merged.reserve(legs.size());
    for (const Leg& leg : legs) {
        const QString from = cwStation::canonicalKey(leg.from);
        const QString to = cwStation::canonicalKey(leg.to);
        const StationPair pair {leg.caveId, std::minmax(from, to)};
        const auto found = firstLeg.constFind(pair);
        if (found == firstLeg.constEnd()) {
            firstLeg.insert(pair, merged.size());
            merged.append(leg);
            continue;
        }

        Leg& first = merged[found.value()];
        const bool isSameWay = cwStation::canonicalKey(first.from) == from;
        const QVector3D& atFirstFrom = isSameWay ? leg.fromPosition : leg.toPosition;
        const QVector3D& atFirstTo = isSameWay ? leg.toPosition : leg.fromPosition;
        const bool isSamePassage = first.fromPosition.distanceToPoint(atFirstFrom) <= kRepeatedShotToleranceMeters
                                   && first.toPosition.distanceToPoint(atFirstTo) <= kRepeatedShotToleranceMeters;
        if (!isSamePassage) {
            merged.append(leg);
            continue;
        }

        first.fromLrud = meanLrud(first.fromLrud, isSameWay ? leg.fromLrud : leg.toLrud);
        first.toLrud = meanLrud(first.toLrud, isSameWay ? leg.toLrud : leg.fromLrud);
        const QString message = first.tripId == leg.tripId
            ? QStringLiteral("The shot from %1 to %2 was surveyed twice in trip %3; the tube draws it once.")
                  .arg(first.from, first.to, tripNames.value(leg.tripId))
            : QStringLiteral("The shot from %1 to %2 was surveyed in trip %3 and again in trip %4; the tube draws it once.")
                  .arg(first.from, first.to, tripNames.value(first.tripId), tripNames.value(leg.tripId));
        warnings.append(Warning {Warning::RepeatedShot, leg.tripId, leg.from, message});
    }
    return merged;
}

QVector<Leg> extractLegs(const cwCavingRegionData& region,
                         const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                         QVector<Warning>& warnings)
{
    QVector<Leg> legs;
    QHash<QUuid, QString> tripNames;
    for (const cwCaveData& cave : region.caves) {
        const auto positionsIt = positionsByCave.constFind(cave.id);
        if (positionsIt == positionsByCave.constEnd()) {
            continue;
        }
        const cwStationPositionLookup& positions = positionsIt.value();

        for (const cwTripData& trip : cave.trips) {
            tripNames.insert(trip.id, trip.name);
            const cwUnits::LengthUnit unit = trip.calibrations.distanceUnit();
            for (int chunkIndex = 0; chunkIndex < trip.chunks.size(); ++chunkIndex) {
                const cwSurveyChunkData& chunk = trip.chunks.at(chunkIndex);

                QVector<cwStationLrud> lruds;
                lruds.reserve(chunk.stations.size());
                for (const cwStation& station : chunk.stations) {
                    lruds.append(stationLrud(station, unit, trip.id, warnings));
                }
                const QVector<QVector3D> facings = chunkFacings(chunk, positions);

                const int shotCount = std::min<int>(chunk.shots.size(), chunk.stations.size() - 1);
                for (int shotIndex = 0; shotIndex < shotCount; ++shotIndex) {
                    const cwStation& from = chunk.stations.at(shotIndex);
                    const cwStation& to = chunk.stations.at(shotIndex + 1);
                    if (!isPositioned(from, positions) || !isPositioned(to, positions)) {
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
                        lruds.at(shotIndex),
                        lruds.at(shotIndex + 1),
                        lruds.at(shotIndex),
                        lruds.at(shotIndex + 1),
                        facings.at(shotIndex),
                        facings.at(shotIndex + 1)
                    });
                }
            }
        }
    }
    return mergeRepeatedShots(legs, tripNames, warnings);
}

std::optional<double> median(QVector<double> values)
{
    if (values.isEmpty()) {
        return std::nullopt;
    }
    std::sort(values.begin(), values.end());
    const int middle = values.size() / 2;
    if (values.size() % 2 == 1) {
        return values.at(middle);
    }
    return (values.at(middle - 1) + values.at(middle)) / 2.0;
}

using StationKey = QPair<QUuid, QString>;

// The ups and downs at the far end of every leg at a station.
struct NeighborReadings {
    QVector<double> ups;
    QVector<double> downs;
};

QHash<StationKey, NeighborReadings> neighborReadings(const QVector<Leg>& legs)
{
    QHash<StationKey, NeighborReadings> readings;
    const auto add = [&](const StationKey& station, const cwStationLrud& farEnd) {
        NeighborReadings& neighbors = readings[station];
        if (farEnd.up) {
            neighbors.ups.append(*farEnd.up);
        }
        if (farEnd.down) {
            neighbors.downs.append(*farEnd.down);
        }
    };
    for (const Leg& leg : legs) {
        add({leg.caveId, leg.from}, leg.toLrud);
        add({leg.caveId, leg.to}, leg.fromLrud);
    }
    return readings;
}

// An up or down far beyond both the passage's width and the same reading at
// the neighboring stations is a pit depth or aven height. The passage LRUD
// take the neighbors' reading in its place, or half the width when they have
// none, so the shaft sizes no leg when chains are chosen.
std::optional<double> shaftReplacement(const std::optional<double>& reading,
                                       const std::optional<double>& width,
                                       const std::optional<double>& neighbor)
{
    const bool hasReference = width || neighbor;
    if (reading && hasReference) {
        const double reference = std::max(width.value_or(0.0), neighbor.value_or(0.0));
        if (*reading > kShaftRatio * reference) {
            return neighbor ? *neighbor : *width / 2.0;
        }
    }
    return std::nullopt;
}

void replaceShaftReadings(QVector<Leg>& legs, QVector<Warning>& warnings)
{
    const QHash<StationKey, NeighborReadings> neighbors = neighborReadings(legs);

    // A station occurrence ends two legs of its chunk; it warns once.
    using Occurrence = std::pair<QUuid, std::pair<int, int>>;
    QSet<Occurrence> visited;

    const auto passageLrud = [&](const Leg& leg, const QString& station, const cwStationLrud& lrud, int stationIndex) {
        const Occurrence occurrence {leg.tripId, {leg.chunkIndex, stationIndex}};
        const bool isFirstVisit = !visited.contains(occurrence);
        visited.insert(occurrence);

        const std::optional<double> width = lrud.left || lrud.right
            ? std::optional<double>(lrud.left.value_or(0.0) + lrud.right.value_or(0.0))
            : std::nullopt;
        const NeighborReadings near = neighbors.value({leg.caveId, station});

        cwStationLrud passage = lrud;
        const auto replaceShaft = [&](std::optional<double>& reading,
                                      const QVector<double>& neighborValues,
                                      const QString& readingName,
                                      const QString& shaftName) {
            const std::optional<double> replacement = shaftReplacement(reading, width, median(neighborValues));
            if (!replacement) {
                return;
            }
            if (isFirstVisit) {
                warnings.append(Warning {
                    Warning::ShaftReading,
                    leg.tripId,
                    station,
                    QStringLiteral("Station %1's %2 of %3 m reads as %4, which the tube draws from the station at face value.")
                        .arg(station, readingName)
                        .arg(*reading)
                        .arg(shaftName)
                });
            }
            reading = replacement;
        };
        replaceShaft(passage.up, near.ups, QStringLiteral("up"), QStringLiteral("an aven"));
        replaceShaft(passage.down, near.downs, QStringLiteral("down"), QStringLiteral("a pit"));
        return passage;
    };

    for (Leg& leg : legs) {
        leg.fromPassageLrud = passageLrud(leg, leg.from, leg.fromLrud, leg.shotIndex);
        leg.toPassageLrud = passageLrud(leg, leg.to, leg.toLrud, leg.shotIndex + 1);
    }
}

// A leg's cross-section where it meets a station, from its passage LRUD
// there, a missing reading taking the default radius.
struct LegSection {
    double halfWidth = 0.0;
    double halfHeight = 0.0;
    double largestReading = 0.0;

    LegSection(const cwStationLrud& lrud, double defaultRadius) :
        halfWidth((lrud.left.value_or(defaultRadius) + lrud.right.value_or(defaultRadius)) / 2.0),
        halfHeight((lrud.up.value_or(defaultRadius) + lrud.down.value_or(defaultRadius)) / 2.0),
        largestReading(std::max({lrud.left.value_or(defaultRadius), lrud.right.value_or(defaultRadius),
                          lrud.up.value_or(defaultRadius), lrud.down.value_or(defaultRadius)}))
    {}

    double area() const { return std::numbers::pi * halfWidth * halfHeight; }

    bool isWiderAndTallerThan(const LegSection& other) const
    {
        return halfWidth > other.halfWidth && halfHeight > other.halfHeight;
    }

    bool isAtLeastAsWideAndTallAs(const LegSection& other) const
    {
        return halfWidth >= other.halfWidth && halfHeight >= other.halfHeight;
    }
};

ChainLeg reversedLeg(const ChainLeg& chainLeg)
{
    return ChainLeg {chainLeg.legIndex, !chainLeg.reversed};
}

// Chains run like Loch's: from each dead end the walk continues along the
// straightest unused leg turning less than kMaxChainTurnDegrees. At a
// junction it continues along the unused leg turning that little whose
// section there is closest to the arriving leg's, ties by straightness. A
// sharper turn, a hairpin, ends the chain: both legs close with domes that
// overlap around the station. At a junction where two legs are bigger than
// the rest, the two of them turning least into each other are its through,
// and a walk arriving on any other leg ends there. Where no two are but one
// leg is, that leg is the through and ends past the junction.
QVector<Chain> buildChains(const QVector<Leg>& legs, double defaultRadius)
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

    const auto sectionAt = [&](int legIndex, int node) {
        const Leg& leg = legs.at(legIndex);
        return LegSection(toNode.at(legIndex) == node ? leg.toPassageLrud : leg.fromPassageLrud, defaultRadius);
    };

    // The leg the walk continues along from arriving at node, among those
    // isCandidate accepts, or -1 when none is.
    const float minTurnCosine = static_cast<float>(std::cos(qDegreesToRadians(kMaxChainTurnDegrees)));
    const auto continuation = [&](int node, const ChainLeg& arriving, const auto& isCandidate) {
        const QVector3D incoming = direction(arriving);
        const double arrivingArea = sectionAt(arriving.legIndex, node).area();
        const auto straightness = [&](int legIndex) {
            return QVector3D::dotProduct(incoming, direction(leaving(legIndex, node)));
        };
        const auto areaGap = [&](int legIndex) {
            return std::abs(sectionAt(legIndex, node).area() - arrivingArea);
        };

        const bool isJunction = degree(node) > 2;
        int next = -1;
        for (int legIndex : legsByNode.at(node)) {
            if (legIndex == arriving.legIndex || !isCandidate(legIndex) || straightness(legIndex) <= minTurnCosine) {
                continue;
            }
            const bool isStraighter = next < 0 || straightness(legIndex) > straightness(next);
            const bool isBetter = isJunction
                                      ? next < 0 || areaGap(legIndex) < areaGap(next) || (areaGap(legIndex) == areaGap(next) && isStraighter)
                                      : isStraighter;
            if (isBetter) {
                next = legIndex;
            }
        }
        return next;
    };

    const auto anyLeg = [](int) { return true; };

    // The through pair of each junction, or -1 where none is: the two legs
    // turning least into each other, within kMaxChainTurnDegrees, among the
    // pairs whose legs are each at least as wide and tall as every other leg
    // there, one of them wider and taller than one of those. A pair that
    // leaves an arm beside a bigger arm is taken only when no other pair is.
    QVector<std::pair<int, int>> throughPairs(legsByNode.size(), {-1, -1});
    QVector<double> throughReach(legsByNode.size(), 0.0);
    const float minArmSpreadCosine = static_cast<float>(std::cos(qDegreesToRadians(kMinArmSpreadDegrees)));
    const double minThroughReachSine = std::sin(qDegreesToRadians(kMinThroughReachDegrees));
    for (int node = 0; node < legsByNode.size(); ++node) {
        if (degree(node) <= 2) {
            continue;
        }
        const QVector<int>& nodeLegs = legsByNode.at(node);
        const auto isBigPair = [&](int a, int b) {
            const LegSection sectionA = sectionAt(a, node);
            const LegSection sectionB = sectionAt(b, node);
            bool isBiggerThanAny = false;
            for (int other : nodeLegs) {
                if (other == a || other == b) {
                    continue;
                }
                const LegSection otherSection = sectionAt(other, node);
                if (!sectionA.isAtLeastAsWideAndTallAs(otherSection) || !sectionB.isAtLeastAsWideAndTallAs(otherSection)) {
                    return false;
                }
                isBiggerThanAny = isBiggerThanAny || sectionA.isWiderAndTallerThan(otherSection) || sectionB.isWiderAndTallerThan(otherSection);
            }
            return isBiggerThanAny;
        };
        const auto leavesArmBesideBiggerArm = [&](int a, int b) {
            for (int arm : nodeLegs) {
                for (int bigger : nodeLegs) {
                    const bool isArmPair = arm != a && arm != b && bigger != a && bigger != b && arm != bigger;
                    if (isArmPair
                        && sectionAt(bigger, node).isWiderAndTallerThan(sectionAt(arm, node))
                        && QVector3D::dotProduct(direction(leaving(arm, node)), direction(leaving(bigger, node))) > minArmSpreadCosine) {
                        return true;
                    }
                }
            }
            return false;
        };

        std::optional<std::pair<bool, float>> bestRank;
        for (int i = 0; i < nodeLegs.size(); ++i) {
            const int a = nodeLegs.at(i);
            const QVector3D incoming = direction(reversedLeg(leaving(a, node)));
            for (int k = i + 1; k < nodeLegs.size(); ++k) {
                const int b = nodeLegs.at(k);
                const float straightness = QVector3D::dotProduct(incoming, direction(leaving(b, node)));
                if (straightness <= minTurnCosine || !isBigPair(a, b)) {
                    continue;
                }
                const bool isUncrowded = !leavesArmBesideBiggerArm(a, b);
                const std::pair<bool, float> rank {isUncrowded, straightness};
                if (!bestRank || rank > *bestRank) {
                    bestRank = rank;
                    throughPairs[node] = {a, b};
                }
            }
        }
        if (bestRank) {
            continue;
        }

        // Without a pair, a leg wider and taller than every other still
        // claims the through and ends past the junction, far enough that
        // every other leg's mouth lands on its wall.
        const auto isBiggest = [&](int leg) {
            return std::all_of(nodeLegs.cbegin(), nodeLegs.cend(), [&](int other) {
                return other == leg || sectionAt(leg, node).isWiderAndTallerThan(sectionAt(other, node));
            });
        };
        const auto biggest = std::find_if(nodeLegs.cbegin(), nodeLegs.cend(), isBiggest);
        if (biggest == nodeLegs.cend()) {
            continue;
        }
        throughPairs[node] = {*biggest, -1};
        const QVector3D throughDirection = direction(leaving(*biggest, node));
        double reach = kThroughReachMarginMeters;
        for (int leg : nodeLegs) {
            if (leg != *biggest) {
                const double sine = QVector3D::crossProduct(throughDirection, direction(leaving(leg, node))).length();
                reach += sectionAt(leg, node).largestReading / std::max(sine, minThroughReachSine);
            }
        }
        throughReach[node] = reach;
    }

    // The other leg of the node's through pair, or -1 when legIndex is not in
    // it.
    const auto throughPartner = [&](int node, int legIndex) {
        const auto [first, second] = throughPairs.at(node);
        if (legIndex == first) {
            return second;
        }
        return legIndex == second ? first : -1;
    };
    const auto isUnused = [&](int legIndex) {
        return !legUsed.at(legIndex);
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
            const bool hasThroughPair = throughPairs.at(node).first >= 0;
            const int nextLeg = hasThroughPair ? throughPartner(node, current.legIndex) : continuation(node, current, isUnused);
            if (nextLeg < 0 || legUsed.at(nextLeg)) {
                break;
            }
            if (isJunction) {
                junctionPassed[node] = true;
            }
            current = leaving(nextLeg, node);
        }
        return chain;
    };

    // A chain starting at a junction runs back first along the other leg of
    // its big passage's through pair, or else along its straightest unused
    // continuation, so a chain passes the junction even when no walk reached
    // it first.
    const auto backwardLeg = [&](int startNode, int firstLeg) {
        if (throughPairs.at(startNode).first >= 0) {
            return throughPartner(startNode, firstLeg);
        }
        const bool isJunction = degree(startNode) > 2;
        return isJunction ? continuation(startNode, reversedLeg(leaving(firstLeg, startNode)), isUnused) : -1;
    };
    const auto walkFrom = [&](int startNode, int firstLeg) {
        const int partner = backwardLeg(startNode, firstLeg);
        if (partner < 0 || legUsed.at(partner) || junctionPassed.at(startNode)) {
            return walkChain(startNode, firstLeg);
        }
        junctionPassed[startNode] = true;
        const Chain backward = walkChain(startNode, partner);
        if (legUsed.at(firstLeg)) {
            return backward;
        }
        Chain chain;
        for (auto it = backward.legs.crbegin(); it != backward.legs.crend(); ++it) {
            chain.legs.append(reversedLeg(*it));
        }
        chain.legs.append(walkChain(startNode, firstLeg).legs);
        return chain;
    };

    const auto isHairpin = [&](int node) {
        const QVector<int>& nodeLegs = legsByNode.at(node);
        return continuation(node, reversedLeg(leaving(nodeLegs.first(), node)), anyLeg) < 0;
    };

    // Dead ends and hairpins first, then odd junctions, then even junctions,
    // so through chains run end to end before arms claim the junctions.
    QVector<int> seeds;
    for (int node = 0; node < legsByNode.size(); ++node) {
        if (degree(node) != 2 || isHairpin(node)) {
            seeds.append(node);
        }
    }
    const auto seedRank = [&](int node) {
        if (degree(node) <= 2) {
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
                chains.append(walkFrom(node, legIndex));
            }
        }
    }

    // What remains are loops made only of degree-2 stations without a hairpin.
    for (int legIndex = 0; legIndex < legs.size(); ++legIndex) {
        if (!legUsed.at(legIndex)) {
            chains.append(walkChain(fromNode.at(legIndex), legIndex));
        }
    }

    const auto reachAt = [&](int node, int legIndex) {
        return throughPairs.at(node) == std::pair<int, int> {legIndex, -1} ? throughReach.at(node) : 0.0;
    };
    for (Chain& chain : chains) {
        const ChainLeg& first = chain.legs.first();
        const ChainLeg& last = chain.legs.last();
        chain.startThroughReachMeters = reachAt(endNode(reversedLeg(first)), first.legIndex);
        chain.endThroughReachMeters = reachAt(endNode(last), last.legIndex);
    }

    return chains;
}

using Frame = cwPassageTubeBuilder::Frame;
using Ring = cwPassageTubeBuilder::Ring;
using Section = cwPassageTubeBuilder::Section;
using Options = cwPassageTubeBuilder::Options;
using EndCap = cwPassageTubeBuilder::EndCap;

bool isSteep(const QVector3D& direction)
{
    return std::abs(direction.normalized().z()) > std::cos(qDegreesToRadians(kSteepLegDegrees));
}

QVector3D inPlan(const QVector3D& v)
{
    return QVector3D(v.x(), v.y(), 0.0f);
}

// The level direction a quarter turn clockwise of direction, seen from above.
QVector3D clockwiseOf(const QVector3D& direction)
{
    return QVector3D(direction.y(), -direction.x(), 0.0f).normalized();
}

// A chain leg as walked: start and end in travel order, with the record the
// leg's own chunk took at each and the facing it was read along.
struct WalkedLeg {
    QVector3D start;
    QVector3D end;
    cwStationLrud startLrud;
    cwStationLrud endLrud;
    QVector3D startFacing;
    QVector3D endFacing;
    QUuid tripId;

    QVector3D direction() const { return (end - start).normalized(); }
    double length() const { return start.distanceToPoint(end); }
};

WalkedLeg walkedLeg(const QVector<Leg>& legs, const ChainLeg& chainLeg)
{
    const Leg& leg = legs.at(chainLeg.legIndex);
    if (chainLeg.reversed) {
        return {leg.toPosition, leg.fromPosition, leg.toLrud, leg.fromLrud, leg.toFacing, leg.fromFacing, leg.tripId};
    }
    return {leg.fromPosition, leg.toPosition, leg.fromLrud, leg.toLrud, leg.fromFacing, leg.toFacing, leg.tripId};
}

bool isLoop(const QVector<WalkedLeg>& walked)
{
    return walked.size() > 1 && walked.first().start == walked.last().end;
}

double smoothstep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double inclinationWeight(const QVector3D& tangent, const Options& options)
{
    const double inclination = std::asin(std::min(1.0, static_cast<double>(std::abs(tangent.z()))));
    return smoothstep(qDegreesToRadians(options.blendLowDegrees),
                      qDegreesToRadians(options.blendHighDegrees),
                      inclination);
}

// A missing wall takes the mean of the walls beside it around the section: a
// side wall of up and down, a ceiling or floor of left and right.
cwStationLrud filledReadings(const cwStationLrud& lrud)
{
    return {
        lrud.left ? lrud.left : meanReading(lrud.up, lrud.down),
        lrud.right ? lrud.right : meanReading(lrud.up, lrud.down),
        lrud.up ? lrud.up : meanReading(lrud.left, lrud.right),
        lrud.down ? lrud.down : meanReading(lrud.left, lrud.right)
    };
}

// Bin k's eccentric anomaly around the section, 2πk/N.
double binParameter(int bin, int resolution)
{
    return kTwoPi * bin / resolution;
}

// The quadrant's horizontal and vertical readings at eccentric anomaly t.
std::pair<std::optional<double>, std::optional<double>> quadrantReadings(const cwStationLrud& lrud, double t)
{
    return {std::cos(t) >= 0.0 ? lrud.right : lrud.left, std::sin(t) >= 0.0 ? lrud.up : lrud.down};
}

// Whether each bin's walls were read: the horizontal reading of its side
// unless it lies on the vertical axis, and the vertical one of its side
// unless it lies on the horizontal axis.
QVector<bool> measuredBins(const cwStationLrud& read, int resolution)
{
    QVector<bool> measured(resolution, false);
    for (int bin = 0; bin < resolution; ++bin) {
        const double t = binParameter(bin, resolution);
        const auto [horizontal, vertical] = quadrantReadings(read, t);
        const bool usesHorizontal = std::abs(std::cos(t)) > kOnAxisMeters;
        const bool usesVertical = std::abs(std::sin(t)) > kOnAxisMeters;
        measured[bin] = (!usesHorizontal || horizontal) && (!usesVertical || vertical);
    }
    return measured;
}

// The axes a section lies on as a leg reads a station's record, and the
// record's walls along them: left and right along -right and right, up and
// down along up and -up. Either way up × right runs along the walk.
struct SectionAxes {
    QVector3D right;
    QVector3D up;
    cwStationLrud walls;

    QVector3D tangent() const { return QVector3D::crossProduct(up, right); }
};

// How a leg walked along direction reads a record taken looking along
// facing (T07ah rule 1). A gentle leg's section stands plumb: right is the
// facing turned clockwise, reversed with the walls it reads when the record
// faces against the leg, or the leg's own right when the facing lies more
// than kMaxFacingDegrees off the leg. A steep leg's section lies level
// (T07ah rule 2): left and right across the facing, front and back along it
// at the mean of left and right. Its right turns with the walls it reads to
// agree with previousRight, the right of the section before it along the
// chain, so the loft between them joins each bin to its match.
SectionAxes sectionAxes(const cwStationLrud& record,
                        const QVector3D& facing,
                        const QVector3D& direction,
                        const std::optional<QVector3D>& previousRight)
{
    if (isSteep(direction)) {
        const QVector3D facingRight = clockwiseOf(facing);
        const bool turnsAgainst = previousRight && QVector3D::dotProduct(inPlan(*previousRight), facingRight) < 0.0f;
        const QVector3D right = turnsAgainst ? -facingRight : facingRight;
        const QVector3D walkUp = direction.z() < 0.0f ? -kWorldUp : kWorldUp;
        const std::optional<double> halfDepth = meanReading(record.left, record.right);
        const std::optional<double>& leftWall = turnsAgainst ? record.right : record.left;
        const std::optional<double>& rightWall = turnsAgainst ? record.left : record.right;
        return SectionAxes {right, QVector3D::crossProduct(right, walkUp), {leftWall, rightWall, halfDepth, halfDepth}};
    }

    const QVector3D squareRight = clockwiseOf(direction);
    const QVector3D facingRight = clockwiseOf(facing);
    const float component = QVector3D::dotProduct(facingRight, squareRight);
    const bool facesAgainst = component < 0.0f;
    const bool readsAlongFacing = std::abs(component) >= std::cos(qDegreesToRadians(cwPassageTubeBuilder::kMaxFacingDegrees));
    const QVector3D right = readsAlongFacing ? facingRight * (facesAgainst ? -1.0f : 1.0f) : squareRight;
    return SectionAxes {right,
                        kWorldUp,
                        {facesAgainst ? record.right : record.left, facesAgainst ? record.left : record.right, record.up, record.down}};
}

// The walls a section draws, a missing one filled from those beside it, or
// nullopt when the record reads too little for a section: no side wall, or
// neither ceiling nor floor.
std::optional<cwStationLrud> drawnWalls(const cwStationLrud& walls)
{
    if ((walls.left || walls.right) && (walls.up || walls.down)) {
        return filledReadings(walls);
    }
    return std::nullopt;
}

// The section's points: the quadrant ellipse of walls around station at
// each bin's eccentric anomaly.
QVector<QVector3D> sectionPoints(const QVector3D& station, const SectionAxes& axes, const cwStationLrud& walls, int resolution)
{
    QVector<QVector3D> points;
    points.reserve(resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        const double t = binParameter(bin, resolution);
        const auto [horizontal, vertical] = quadrantReadings(walls, t);
        points.append(station
                      + axes.right * static_cast<float>(*horizontal * std::cos(t))
                      + axes.up * static_cast<float>(*vertical * std::sin(t)));
    }
    return points;
}

bool isSameSection(const QVector<QVector3D>& a, const QVector<QVector3D>& b)
{
    for (int bin = 0; bin < a.size(); ++bin) {
        if (a.at(bin).distanceToPoint(b.at(bin)) >= kMinRingSpacingMeters) {
            return false;
        }
    }
    return true;
}

// One end of a chain leg: the station, how its record reads across the leg,
// and the walls the section draws, borrowed when it reads too little.
struct LegEnd {
    QVector3D station;
    double distanceAlongChain = 0.0;
    int chainLeg = 0;
    QUuid tripId;
    SectionAxes axes;
    std::optional<cwStationLrud> walls;
    bool isBorrowed = false;
    bool isSteep = false;
};

// A leg end's section as a ring's vertices and frame, with the bins its
// readings measured.
struct EndSection {
    QVector3D station;
    QVector<QVector3D> points;
    QVector3D right;
    QVector3D tangent;
    QVector<bool> measured;
    std::optional<cwStationLrud> readings;
};

// Level dead ends sit flush with the end station; pits extend by up or down.
double capOffset(const cwStationLrud& lrud, const QVector3D& outward, const Options& options)
{
    const std::optional<double> offset = outward.z() >= 0.0f ? lrud.up : lrud.down;
    return inclinationWeight(outward, options) * offset.value_or(0.0);
}

// A warning about the chain, reported at its first station. message takes
// the station name as %1.
Warning chainWarning(const QVector<Leg>& legs, const Chain& chain, Warning::Type type, const QString& message)
{
    const ChainLeg& firstLeg = chain.legs.first();
    const QString station = chainStartStation(legs, firstLeg);
    return Warning {type, legs.at(firstLeg.legIndex).tripId, station, message.arg(station)};
}

// Each leg end's walls, as its own record reads them or, when it reads too
// little, borrowed from the nearest leg end of the chain that reads enough,
// first from its own trip within kSameTripBorrowLegs legs; a steep leg's end
// keeps their left and right and reads front and back at the mean of them.
// ends run in chain order, each leg's start before its end. Without any, the
// sections are round at the default radius and the chain warns.
void borrowWalls(QVector<LegEnd>& ends, const QVector<Leg>& legs, const Chain& chain, const Options& options, QVector<Warning>& warnings)
{
    QVector<int> read;
    for (int i = 0; i < ends.size(); ++i) {
        if (ends.at(i).walls) {
            read.append(i);
        }
    }
    if (read.isEmpty()) {
        warnings.append(chainWarning(legs, chain, Warning::NoPassageData,
                                     QStringLiteral("The passage from station %1 has no LRUD; the tube uses the default radius.")));
        const double radius = options.defaultRadiusMeters;
        for (LegEnd& end : ends) {
            end.walls = cwStationLrud {radius, radius, radius, radius};
            end.isBorrowed = true;
        }
        return;
    }

    const QVector<LegEnd> lent = ends;
    for (int i = 0; i < ends.size(); ++i) {
        LegEnd& end = ends[i];
        if (end.walls) {
            continue;
        }
        end.isBorrowed = true;
        // The legs either side of a station the chain passes read one record
        // there, so they borrow alike.
        const bool isLegStart = i % 2 == 0;
        if (isLegStart && i > 0 && !lent.at(i - 1).walls) {
            end.walls = ends.at(i - 1).walls;
            continue;
        }
        QVector<int> sameTrip;
        std::copy_if(read.cbegin(), read.cend(), std::back_inserter(sameTrip), [&](int other) {
            return lent.at(other).tripId == end.tripId && std::abs(lent.at(other).chainLeg - end.chainLeg) <= kSameTripBorrowLegs;
        });
        const QVector<int>& candidates = sameTrip.isEmpty() ? read : sameTrip;
        const int nearest = *std::min_element(candidates.cbegin(), candidates.cend(), [&](int a, int b) {
            return std::abs(lent.at(a).distanceAlongChain - end.distanceAlongChain)
                   < std::abs(lent.at(b).distanceAlongChain - end.distanceAlongChain);
        });
        end.walls = lent.at(nearest).walls;
    }

    for (LegEnd& end : ends) {
        if (end.isBorrowed && end.isSteep) {
            const std::optional<double> halfDepth = meanReading(end.walls->left, end.walls->right);
            end.walls = cwStationLrud {end.walls->left, end.walls->right, halfDepth, halfDepth};
        }
    }
}

EndSection endSection(const LegEnd& end, int resolution)
{
    const bool hasOwnSection = !end.isBorrowed && !end.isSteep;
    return EndSection {end.station,
                       sectionPoints(end.station, end.axes, *end.walls, resolution),
                       end.axes.right,
                       end.axes.tangent(),
                       end.isBorrowed ? QVector<bool>(resolution, false) : measuredBins(end.axes.walls, resolution),
                       hasOwnSection ? end.walls : std::nullopt};
}

// The normal of the plane that best fits a ring's points (Newell's method),
// signed as a section's tangent, so its bins run from right toward up around
// it. nullopt for points without area.
std::optional<QVector3D> facingOf(const QVector<QVector3D>& points, const QVector3D& center)
{
    QVector3D normal;
    for (int k = 0; k < points.size(); ++k) {
        normal += QVector3D::crossProduct(points.at((k + 1) % points.size()) - center, points.at(k) - center);
    }
    if (normal.lengthSquared() < kDegenerateLengthSquared) {
        return std::nullopt;
    }
    return normal.normalized();
}

QVector<QVector3D> translated(const QVector<QVector3D>& points, const QVector3D& offset)
{
    QVector<QVector3D> moved;
    moved.reserve(points.size());
    for (const QVector3D& point : points) {
        moved.append(point + offset);
    }
    return moved;
}

// Lofts each leg between its two stations' sections (T07ah rule 1): the
// rings are the sections at the stations and their point-wise lerps evenly
// along the leg. Legs that read the same section at a station share its
// ring; where they read different ones, the chain holds both rings there. A
// dome's end ring sits past its station by the station's half-width, and a
// through's past its junction by the arms' reach.
void buildRings(const QVector<Leg>& legs, Chain& chain, const Options& options, QVector<Warning>& warnings)
{
    QVector<WalkedLeg> walked;
    walked.reserve(chain.legs.size());
    for (const ChainLeg& chainLeg : std::as_const(chain.legs)) {
        walked.append(walkedLeg(legs, chainLeg));
    }
    const bool loop = isLoop(walked);
    const int resolution = std::max(options.ringResolution, 1);
    const double spacing = std::max(options.sampleSpacingMeters, kMinSampleSpacingMeters);
    const int legCount = walked.size();

    QVector<LegEnd> ends;
    ends.reserve(2 * legCount);
    double distance = 0.0;
    std::optional<QVector3D> previousRight;
    for (int i = 0; i < legCount; ++i) {
        const WalkedLeg& leg = walked.at(i);
        const QVector3D direction = leg.direction();
        const bool steep = isSteep(direction);
        const SectionAxes startAxes = sectionAxes(leg.startLrud, leg.startFacing, direction, previousRight);
        const SectionAxes endAxes = sectionAxes(leg.endLrud, leg.endFacing, direction, startAxes.right);
        ends.append(LegEnd {leg.start, distance, i, leg.tripId, startAxes, drawnWalls(startAxes.walls), false, steep});
        distance += leg.length();
        ends.append(LegEnd {leg.end, distance, i, leg.tripId, endAxes, drawnWalls(endAxes.walls), false, steep});
        previousRight = endAxes.right;
    }
    borrowWalls(ends, legs, chain, options, warnings);

    // A ring closer than kMinRingSpacingMeters to the one before merges into
    // it, unless both are sections of one station.
    chain.rings.clear();
    const auto appendRing = [&](const QVector3D& center,
                                const QVector<QVector3D>& points,
                                const QVector3D& tangent,
                                const QVector3D& rightHint,
                                int chainLeg,
                                const QVector<bool>& measured) -> Ring* {
        if (!chain.rings.isEmpty()) {
            const QVector3D& previous = chain.rings.constLast().center;
            if (previous != center && previous.distanceToPoint(center) < kMinRingSpacingMeters) {
                return nullptr;
            }
        }
        Ring ring;
        ring.center = center;
        ring.chainLeg = chainLeg;
        ring.measured = measured;
        cwPassageTubeGeometry::setRingVertices(ring, points, tangent, rightHint, inclinationWeight(walked.at(chainLeg).direction(), options));
        chain.rings.append(ring);
        return &chain.rings.last();
    };
    const auto appendSection = [&](const EndSection& end, int chainLeg) {
        Ring* ring = appendRing(end.station, end.points, end.tangent, end.right, chainLeg, end.measured);
        if (ring != nullptr && end.readings) {
            ring->section = Section {*end.readings, ring->radii, end.station};
        }
    };
    const auto appendOffset = [&](const EndSection& end, int chainLeg, const QVector3D& offset) {
        appendRing(end.station + offset, translated(end.points, offset), end.tangent, end.right, chainLeg, QVector<bool>(resolution, false));
    };

    const auto endOffset = [&](EndCap cap, const LegEnd& end, double throughReach) {
        if (cap != EndCap::Dome) {
            return throughReach;
        }
        const cwStationLrud& walls = *end.walls;
        return (*walls.left + *walls.right) / 2.0;
    };

    std::optional<EndSection> previousEnd;
    for (int i = 0; i < legCount; ++i) {
        const WalkedLeg& leg = walked.at(i);
        const EndSection start = endSection(ends.at(2 * i), resolution);
        const EndSection end = endSection(ends.at(2 * i + 1), resolution);

        if (i == 0 && !loop) {
            const double offset = endOffset(chain.startCap, ends.first(), chain.startThroughReachMeters);
            if (offset >= kMinRingSpacingMeters) {
                appendOffset(start, i, -leg.direction() * static_cast<float>(offset));
            }
        }
        if (!previousEnd || !isSameSection(previousEnd->points, start.points)) {
            appendSection(start, i);
        }

        const int intervals = std::max(1, static_cast<int>(std::lround(leg.length() / spacing)));
        for (int j = 1; j < intervals; ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(intervals);
            if ((1.0 - t) * leg.length() < kMinRingSpacingMeters) {
                continue;
            }
            QVector<QVector3D> points;
            QVector<bool> measured;
            points.reserve(resolution);
            measured.reserve(resolution);
            for (int bin = 0; bin < resolution; ++bin) {
                points.append(start.points.at(bin) + (end.points.at(bin) - start.points.at(bin)) * t);
                measured.append(start.measured.at(bin) && end.measured.at(bin));
            }
            const QVector3D center = leg.start + (leg.end - leg.start) * t;
            const QVector3D lerpedTangent = start.tangent + (end.tangent - start.tangent) * t;
            appendRing(center,
                       points,
                       facingOf(points, center).value_or(lerpedTangent.lengthSquared() < kDegenerateLengthSquared ? leg.direction() : lerpedTangent),
                       start.right + (end.right - start.right) * t,
                       i,
                       measured);
        }

        appendSection(end, i);
        previousEnd = end;
    }

    if (loop) {
        const EndSection seam = endSection(ends.first(), resolution);
        if (!isSameSection(previousEnd->points, seam.points)) {
            appendSection(seam, legCount - 1);
        }
        chain.startCapOffsetMeters = 0.0;
        chain.endCapOffsetMeters = 0.0;
        return;
    }

    const double offset = endOffset(chain.endCap, ends.last(), chain.endThroughReachMeters);
    if (offset >= kMinRingSpacingMeters) {
        appendOffset(*previousEnd, legCount - 1, walked.last().direction() * static_cast<float>(offset));
    }
    chain.startCapOffsetMeters = capOffset(walked.first().startLrud, -walked.first().direction(), options);
    chain.endCapOffsetMeters = capOffset(walked.last().endLrud, walked.last().direction(), options);
}

StationKey canonicalStation(const QUuid& caveId, const QString& station)
{
    return {caveId, cwStation::canonicalKey(station)};
}

// A chain end at a junction that another chain passes through, or ends past
// as its through, is an arm and gets a mouth; an end at a station with one
// other leg is a hairpin break and gets a dome; every other open end gets a
// fan, and loops get no caps.
void classifyChainEnds(const QVector<Leg>& legs, QVector<Chain>& chains)
{
    const auto key = [&](const ChainLeg& chainLeg, const QString& station) {
        return canonicalStation(legs.at(chainLeg.legIndex).caveId, station);
    };

    QHash<StationKey, int> degree;
    for (const Leg& leg : legs) {
        degree[canonicalStation(leg.caveId, leg.from)]++;
        degree[canonicalStation(leg.caveId, leg.to)]++;
    }

    QSet<StationKey> passedThrough;
    for (const Chain& chain : std::as_const(chains)) {
        for (int i = 1; i < chain.legs.size(); ++i) {
            passedThrough.insert(key(chain.legs.at(i), chainStartStation(legs, chain.legs.at(i))));
        }
        if (chain.startThroughReachMeters > 0.0) {
            passedThrough.insert(key(chain.legs.first(), chainStartStation(legs, chain.legs.first())));
        }
        if (chain.endThroughReachMeters > 0.0) {
            passedThrough.insert(key(chain.legs.last(), chainEndStation(legs, chain.legs.last())));
        }
    }

    const auto endCap = [&](const StationKey& station) {
        const int stationDegree = degree.value(station);
        if (stationDegree == 2) {
            return EndCap::Dome;
        }
        const bool isArm = stationDegree > 2 && passedThrough.contains(station);
        return isArm ? EndCap::Mouth : EndCap::Fan;
    };

    for (Chain& chain : chains) {
        const ChainLeg& first = chain.legs.first();
        const ChainLeg& last = chain.legs.last();
        const StationKey start = key(first, chainStartStation(legs, first));
        const StationKey end = key(last, chainEndStation(legs, last));
        if (chain.legs.size() > 1 && start == end) {
            chain.startCap = EndCap::None;
            chain.endCap = EndCap::None;
        } else {
            chain.startCap = chain.startThroughReachMeters > 0.0 ? EndCap::Fan : endCap(start);
            chain.endCap = chain.endThroughReachMeters > 0.0 ? EndCap::Fan : endCap(end);
        }
    }
}

// Collects triangles per trip so each trip's indices end up contiguous, with
// the trip boundary discs after them, one range per pair of trips, and
// records the chain and cap that emitted each one.
class MeshWriter
{
public:
    explicit MeshWriter(cwPassageTubeBuilder::Debug& debug) :
        m_debug(debug),
        m_mesh(debug.mesh)
    {}

    // The chain the following triangles belong to, and the cap in
    // Debug::caps they make up, -1 for none.
    void setSource(int chain, int cap = -1)
    {
        m_source = {chain, cap};
    }

    // The following triangles, up to endCap, close their trip's tube at
    // station where it runs on into neighborTrip's, and draw only while that
    // trip is hidden.
    void beginTripBoundary(const QUuid& neighborTrip, const QUuid& caveId, const QString& station)
    {
        m_source.cap = m_debug.caps.size();
        m_debug.caps.append(cwPassageTubeBuilder::Cap {m_source.chain, caveId, station});
        m_neighborTrip = neighborTrip;
    }

    void endCap()
    {
        m_source.cap = -1;
        m_neighborTrip.reset();
    }

    void markCapCenter(quint32 index)
    {
        m_debug.intendedCreaseVertices.insert(index);
    }

    quint32 addRing(const QVector<QVector3D>& vertices)
    {
        const quint32 first = static_cast<quint32>(m_mesh.positions.size());
        m_mesh.positions.append(vertices);
        return first;
    }

    void addTriangle(const QUuid& tripId, quint32 a, quint32 b, quint32 c)
    {
        TripTriangles& trip = m_neighborTrip ? boundary(tripId, *m_neighborTrip) : tripTriangles(tripId);
        trip.triangles.append(QVector<quint32> {a, b, c});
        trip.sources.append(m_source);
    }

    void finish()
    {
        const auto append = [&](const TripTriangles& trip) {
            const cwPassageTubeMesh::VertexRange range {static_cast<int>(m_mesh.indices.size()), static_cast<int>(trip.triangles.size())};
            m_mesh.indices.append(trip.triangles);
            for (const Source& source : trip.sources) {
                m_debug.triangleChain.append(source.chain);
                m_debug.triangleInZipBand.append(false);
                m_debug.triangleCap.append(source.cap);
            }
            return range;
        };
        for (const QUuid& tripId : std::as_const(m_tripOrder)) {
            m_mesh.tripIndexRanges.append(append(m_trianglesByTrip.value(tripId)));
            m_mesh.tripUuids.append(tripId);
        }
        for (const TripPair& pair : std::as_const(m_boundaryOrder)) {
            const cwPassageTubeMesh::VertexRange range = append(m_boundaries.value(pair));
            m_mesh.tripBoundaryRanges.append({range, static_cast<int>(m_tripOrder.indexOf(pair.first)),
                                              static_cast<int>(m_tripOrder.indexOf(pair.second))});
        }
    }

private:
    struct Source {
        int chain = -1;
        int cap = -1;
    };

    struct TripTriangles {
        QVector<quint32> triangles;
        QVector<Source> sources;
    };

    // A trip and the neighbor trip its boundary discs close it against.
    using TripPair = std::pair<QUuid, QUuid>;

    TripTriangles& tripTriangles(const QUuid& tripId)
    {
        auto it = m_trianglesByTrip.find(tripId);
        if (it == m_trianglesByTrip.end()) {
            m_tripOrder.append(tripId);
            it = m_trianglesByTrip.insert(tripId, {});
        }
        return it.value();
    }

    TripTriangles& boundary(const QUuid& tripId, const QUuid& neighborTrip)
    {
        tripTriangles(tripId);
        tripTriangles(neighborTrip);
        const TripPair pair {tripId, neighborTrip};
        auto it = m_boundaries.find(pair);
        if (it == m_boundaries.end()) {
            m_boundaryOrder.append(pair);
            it = m_boundaries.insert(pair, {});
        }
        return it.value();
    }

    cwPassageTubeBuilder::Debug& m_debug;
    cwPassageTubeMesh& m_mesh;
    Source m_source;
    std::optional<QUuid> m_neighborTrip;
    QVector<QUuid> m_tripOrder;
    QHash<QUuid, TripTriangles> m_trianglesByTrip;
    QVector<TripPair> m_boundaryOrder;
    QHash<TripPair, TripTriangles> m_boundaries;
};

// The mesh's vertices by position, so a surface written apart from the union
// shares the union's vertices, within kMergeToleranceMeters, along the edges
// where the two meet.
class VertexWelder
{
public:
    VertexWelder(MeshWriter& writer, const QVector<QVector3D>& positions) :
        m_writer(writer),
        m_positions(positions)
    {}

    // Takes in the vertices from first on.
    void addFrom(int first)
    {
        for (int v = first; v < m_positions.size(); ++v) {
            insert(static_cast<quint32>(v));
        }
    }

    // The vertex nearest position within the tolerance, else a new one.
    quint32 vertexAt(const QVector3D& position)
    {
        const Cell center = cellOf(position);
        std::optional<quint32> nearest;
        float nearestDistance = kTolerance;
        for (qint64 dx = -1; dx <= 1; ++dx) {
            for (qint64 dy = -1; dy <= 1; ++dy) {
                for (qint64 dz = -1; dz <= 1; ++dz) {
                    const auto cell = m_cells.constFind(Cell {center.x + dx, center.y + dy, center.z + dz});
                    if (cell == m_cells.cend()) {
                        continue;
                    }
                    for (const quint32 v : cell.value()) {
                        const float distance = m_positions.at(v).distanceToPoint(position);
                        if (distance <= nearestDistance) {
                            nearestDistance = distance;
                            nearest = v;
                        }
                    }
                }
            }
        }
        if (nearest) {
            return *nearest;
        }
        const quint32 added = m_writer.addRing({position});
        insert(added);
        return added;
    }

private:
    static constexpr float kTolerance = static_cast<float>(cwPassageTubeUnion::kMergeToleranceMeters);

    struct Cell {
        qint64 x = 0;
        qint64 y = 0;
        qint64 z = 0;

        bool operator==(const Cell&) const = default;
        friend size_t qHash(const Cell& cell, size_t seed = 0) { return qHashMulti(seed, cell.x, cell.y, cell.z); }
    };

    static Cell cellOf(const QVector3D& position)
    {
        return {static_cast<qint64>(std::floor(position.x() / kTolerance)),
                static_cast<qint64>(std::floor(position.y() / kTolerance)),
                static_cast<qint64>(std::floor(position.z() / kTolerance))};
    }

    void insert(quint32 v)
    {
        m_cells[cellOf(m_positions.at(v))].append(v);
    }

    MeshWriter& m_writer;
    const QVector<QVector3D>& m_positions;
    QHash<Cell, QVector<quint32>> m_cells;
};

// The station a ring closes the chain's tube at: the chain's first station
// for its first ring, else the station ending the ring's leg.
const QString& stationOfRing(const QVector<Leg>& legs, const Chain& chain, int ring)
{
    return ring == 0 ? chainStartStation(legs, chain.legs.first())
                     : chainEndStation(legs, chain.legs.at(chain.rings.at(ring).chainLeg));
}

void warnTornChains(const QVector<Leg>& legs, const QVector<Chain>& chains, QVector<Warning>& warnings)
{
    for (const Chain& chain : chains) {
        const bool isTorn = std::any_of(chain.rings.cbegin(), chain.rings.cend(), [](const Ring& ring) {
            return ring.vertices.isEmpty();
        });
        if (isTorn) {
            warnings.append(chainWarning(legs, chain, Warning::ChainTorn,
                                         QStringLiteral("The passage from station %1 breaks where its cross-section could not be oriented.")));
        }
    }
}

// Warns of each tube that crosses itself; the union takes it as it is.
void warnSelfCrossingTubes(const cwPassageTubeBuilder::Debug& debug,
                           const QVector<cwPassageTubeUnion::ChainTube>& tubes,
                           QVector<Warning>& warnings)
{
    for (const cwPassageTubeUnion::ChainTube& tube : tubes) {
        const int crossings = static_cast<int>(
            cwPassageTubeMeshCheck::selfIntersections(tube.mesh, cwPassageTubeMeshCheck::wholeMesh(tube.mesh)).size());
        if (crossings > 0) {
            Warning warning = chainWarning(debug.legs, debug.chains.at(tube.chain), Warning::ChainSelfCrossing,
                                           QStringLiteral("The passage from station %1 crosses itself at %2 pairs of triangles."));
            warning.message = warning.message.arg(crossings);
            warning.tripId = tube.tripId;
            warnings.append(warning);
        }
    }
}

// The station of the chain's legs nearest point.
const QString& nearestStation(const QVector<Leg>& legs, const Chain& chain, const QVector3D& point)
{
    const QString* nearest = &legs.at(chain.legs.first().legIndex).from;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const ChainLeg& chainLeg : chain.legs) {
        const Leg& leg = legs.at(chainLeg.legIndex);
        for (const auto& [station, position] : {std::pair {&leg.from, leg.fromPosition}, std::pair {&leg.to, leg.toPosition}}) {
            const float distance = position.distanceToPoint(point);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = station;
            }
        }
    }
    return *nearest;
}

QUuid caveOf(const QVector<Leg>& legs, const Chain& chain)
{
    return legs.at(chain.legs.first().legIndex).caveId;
}

// What closes each trip of a group of overlapping tubes while the other
// trips are hidden, as boundary triangles against the trip that covers them:
// the trip's surface inside the other trips' tubes, one cap per tube and
// covering trip at the tube's station nearest it, and the part of each fan
// across a ring a chain is cut at between trips that lies outside the
// trip's own other tubes, the disc at that ring.
void writeTripCovers(cwPassageTubeBuilder::Debug& debug,
                     const QVector<cwPassageTubeUnion::ChainTube>& tubes,
                     const QVector<int>& inputs,
                     MeshWriter& writer,
                     VertexWelder& welder)
{
    using Union = cwPassageTubeUnion;
    QVector<QUuid> trips;
    for (const int input : inputs) {
        if (!trips.contains(tubes.at(input).tripId)) {
            trips.append(tubes.at(input).tripId);
        }
    }
    if (trips.size() < 2) {
        return;
    }

    const auto coveringTrip = [&](const QUuid& trip, const QVector3D& point) {
        std::optional<QUuid> fallback;
        for (const QUuid& other : std::as_const(trips)) {
            if (other == trip) {
                continue;
            }
            fallback = fallback.value_or(other);
            for (const int input : inputs) {
                if (tubes.at(input).tripId == other && Union::contains(tubes.at(input).mesh, point)) {
                    return other;
                }
            }
        }
        return *fallback;
    };

    // The cut ring's neighbor trip of a triangle from a tube's end fan.
    const auto cutNeighborTrip = [](const Union::ChainTube& tube, int face) -> std::optional<QUuid> {
        if (face == Union::kStartCap) {
            return tube.startNeighborTrip;
        }
        if (face == Union::kEndCap) {
            return tube.endNeighborTrip;
        }
        return std::nullopt;
    };

    struct Group {
        int input = -1;
        QUuid neighborTrip;
        std::optional<int> cutRing;
        QVector<int> triangles;
    };

    for (const QUuid& trip : std::as_const(trips)) {
        QVector<int> tripInputs;
        QVector<cwPassageTubeMesh> meshes;
        QVector<QVector<int>> faces;
        QVector<cwPassageTubeMesh> around;
        for (const int input : inputs) {
            const Union::ChainTube& tube = tubes.at(input);
            if (tube.tripId == trip) {
                tripInputs.append(input);
                meshes.append(tube.mesh);
                faces.append(tube.faces());
            } else {
                around.append(tube.mesh);
            }
        }

        const auto write = [&](const Union::Result& source, bool isCutFanSource) {
            const cwPassageTubeMesh& mesh = source.mesh;
            QVector<Group> groups;
            for (int t = 0; t < source.triangleOrigins.size(); ++t) {
                const Union::Origin& origin = source.triangleOrigins.at(t);
                const int input = tripInputs.at(origin.input);
                const Union::ChainTube& tube = tubes.at(input);
                const std::optional<QUuid> cutNeighbor = cutNeighborTrip(tube, origin.face);
                if (cutNeighbor.has_value() != isCutFanSource) {
                    continue;
                }
                std::optional<int> cutRing;
                QUuid neighborTrip;
                if (cutNeighbor) {
                    cutRing = origin.face == Union::kStartCap ? tube.rings.first() : tube.rings.last();
                    neighborTrip = *cutNeighbor;
                } else {
                    const QVector3D center = (mesh.positions.at(mesh.indices.at(3 * t)) + mesh.positions.at(mesh.indices.at(3 * t + 1))
                                              + mesh.positions.at(mesh.indices.at(3 * t + 2))) / 3.0f;
                    neighborTrip = coveringTrip(trip, center);
                }
                auto group = std::find_if(groups.begin(), groups.end(), [&](const Group& g) {
                    return g.input == input && g.neighborTrip == neighborTrip && g.cutRing == cutRing;
                });
                if (group == groups.end()) {
                    groups.append({input, neighborTrip, cutRing, {}});
                    group = std::prev(groups.end());
                }
                group->triangles.append(t);
            }

            QVector<std::optional<quint32>> welded(mesh.positions.size());
            const auto vertexOf = [&](quint32 index) {
                if (!welded.at(index)) {
                    welded[index] = welder.vertexAt(mesh.positions.at(index));
                }
                return *welded.at(index);
            };
            for (const Group& group : std::as_const(groups)) {
                const Chain& chain = debug.chains.at(tubes.at(group.input).chain);
                const auto station = [&]() {
                    if (group.cutRing) {
                        return stationOfRing(debug.legs, chain, *group.cutRing);
                    }
                    QVector3D sum;
                    for (const int t : group.triangles) {
                        for (int k = 0; k < 3; ++k) {
                            sum += mesh.positions.at(mesh.indices.at(3 * t + k));
                        }
                    }
                    return nearestStation(debug.legs, chain, sum / static_cast<float>(3 * group.triangles.size()));
                };
                writer.setSource(tubes.at(group.input).chain);
                writer.beginTripBoundary(group.neighborTrip, caveOf(debug.legs, chain), station());
                for (const int t : group.triangles) {
                    const quint32 a = vertexOf(mesh.indices.at(3 * t));
                    const quint32 b = vertexOf(mesh.indices.at(3 * t + 1));
                    const quint32 c = vertexOf(mesh.indices.at(3 * t + 2));
                    // A sliver whose corners weld together is left out.
                    if (a != b && b != c && c != a) {
                        writer.addTriangle(trip, a, b, c);
                    }
                }
                writer.endCap();
            }
        };

        write(Union::coveredSurface(meshes, faces, around), false);
        const bool hasCutFans = std::any_of(tripInputs.cbegin(), tripInputs.cend(), [&](int input) {
            return tubes.at(input).startNeighborTrip || tubes.at(input).endNeighborTrip;
        });
        if (hasCutFans) {
            write(Union::unite(meshes, faces), true);
        }
    }
}

// The union of each connected group of chains' tubes, written trip by trip
// with each triangle's chain and end fan, then the trip boundary discs where
// a chain runs on from one trip into another.
void writeUnion(cwPassageTubeBuilder::Debug& debug)
{
    using Union = cwPassageTubeUnion;
    const QVector<Union::ChainTube> tubes = Union::chainTubes(debug);
    warnSelfCrossingTubes(debug, tubes, debug.warnings);

    // Each tube's end fans are caps, apart from a dome, which rounds the
    // passage's end: one entry per tube and end, -1 for none.
    QVector<std::array<int, 2>> capOfEnd;
    capOfEnd.reserve(tubes.size());
    for (const Union::ChainTube& tube : tubes) {
        const Chain& chain = debug.chains.at(tube.chain);
        const auto addCap = [&](int ring, bool isDome) {
            if (tube.isLoop || isDome) {
                return -1;
            }
            debug.caps.append({tube.chain, caveOf(debug.legs, chain), stationOfRing(debug.legs, chain, ring)});
            return static_cast<int>(debug.caps.size()) - 1;
        };
        const int lastRing = static_cast<int>(chain.rings.size()) - 1;
        capOfEnd.append({addCap(tube.rings.first(), tube.rings.first() == 0 && chain.startCap == EndCap::Dome),
                         addCap(tube.rings.last(), tube.rings.last() == lastRing && chain.endCap == EndCap::Dome)});
    }

    MeshWriter writer(debug);
    VertexWelder welder(writer, debug.mesh.positions);
    for (const QVector<int>& component : Union::chainComponents(debug)) {
        QVector<int> inputs;
        QVector<cwPassageTubeMesh> meshes;
        QVector<QVector<int>> faces;
        for (int t = 0; t < tubes.size(); ++t) {
            if (component.contains(tubes.at(t).chain)) {
                inputs.append(t);
                meshes.append(tubes.at(t).mesh);
                faces.append(tubes.at(t).faces());
            }
        }
        if (inputs.isEmpty()) {
            continue;
        }

        const Union::Result result = Union::unite(meshes, faces);
        const Chain& firstChain = debug.chains.at(component.first());
        for (int i = 0; i < inputs.size(); ++i) {
            if (result.inputStatus.at(i) != Union::Status::NoError) {
                Warning warning = chainWarning(debug.legs, debug.chains.at(tubes.at(inputs.at(i)).chain), Warning::TubeRejected,
                                               QStringLiteral("The passage from station %1 is left out: its tube is %2."));
                warning.message = warning.message.arg(Union::statusName(result.inputStatus.at(i)));
                debug.warnings.append(warning);
            }
        }
        if (result.status != Union::Status::NoError) {
            Warning warning = chainWarning(debug.legs, firstChain, Warning::TubeRejected,
                                           QStringLiteral("The passages joined to station %1 are left out: their union is %2."));
            warning.message = warning.message.arg(Union::statusName(result.status));
            debug.warnings.append(warning);
            continue;
        }
        for (const Union::Void& pocket : result.voids) {
            Warning warning = chainWarning(debug.legs, firstChain, Warning::EnclosedVoid,
                                           QStringLiteral("The passages joined to station %1 enclose a void of %2 m³ at (%3, %4, %5), dropped from the tube."));
            warning.message = warning.message.arg(-pocket.volume, 0, 'f', 3)
                                  .arg(pocket.centroid.x(), 0, 'f', 2)
                                  .arg(pocket.centroid.y(), 0, 'f', 2)
                                  .arg(pocket.centroid.z(), 0, 'f', 2);
            debug.warnings.append(warning);
        }

        const quint32 offset = writer.addRing(result.mesh.positions);
        welder.addFrom(static_cast<int>(offset));
        const QVector<QVector3D>& positions = debug.mesh.positions;
        for (int t = 0; t < result.triangleOrigins.size(); ++t) {
            const Union::Origin& origin = result.triangleOrigins.at(t);
            const int input = inputs.value(origin.input, inputs.first());
            const Union::ChainTube& tube = tubes.at(input);
            const bool isCap = origin.face == Union::kStartCap || origin.face == Union::kEndCap;
            writer.setSource(tube.chain, isCap ? capOfEnd.at(input).at(origin.face - Union::kStartCap) : -1);
            const std::array<quint32, 3> corners {offset + result.mesh.indices.at(3 * t),
                                                  offset + result.mesh.indices.at(3 * t + 1),
                                                  offset + result.mesh.indices.at(3 * t + 2)};
            if (isCap) {
                const QVector3D center = tube.capCenter(origin.face);
                for (const quint32 corner : corners) {
                    if (positions.at(corner) == center) {
                        writer.markCapCenter(corner);
                    }
                }
            }
            writer.addTriangle(tube.tripId, corners.at(0), corners.at(1), corners.at(2));
        }

        QVector<int> accepted;
        for (int i = 0; i < inputs.size(); ++i) {
            if (result.inputStatus.at(i) == Union::Status::NoError) {
                accepted.append(inputs.at(i));
            }
        }
        writeTripCovers(debug, tubes, accepted, writer, welder);
    }

    writer.finish();
}

} // namespace

cwPassageTubeMesh cwPassageTubeBuilder::build(const cwCavingRegionData& region,
                                              const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                              const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                              const Options& options)
{
    return buildDebug(region, positionsByCave, splaysByCave, options).mesh;
}

cwPassageTubeBuilder::Debug cwPassageTubeBuilder::buildDebug(const cwCavingRegionData& region,
                                                             const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                                             const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                                             const Options& options)
{
    Q_UNUSED(splaysByCave)

    Debug debug = buildChainRings(region, positionsByCave, options);
    warnTornChains(debug.legs, debug.chains, debug.warnings);
    if (options.slideCaps) {
        debug.slidCaps = cwPassageTubeUnion::slideCaps(debug);
    }
    writeUnion(debug);
    debug.mesh.warnings = debug.warnings;
    return debug;
}

cwPassageTubeBuilder::Debug cwPassageTubeBuilder::buildChainRings(const cwCavingRegionData& region,
                                                                  const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                                                  const Options& options)
{
    Debug debug;
    debug.legs = extractLegs(region, positionsByCave, debug.warnings);
    replaceShaftReadings(debug.legs, debug.warnings);
    debug.chains = buildChains(debug.legs, options.defaultRadiusMeters);
    classifyChainEnds(debug.legs, debug.chains);
    for (Chain& chain : debug.chains) {
        buildRings(debug.legs, chain, options, debug.warnings);
        placeChainVertices(chain);
    }
    return debug;
}
