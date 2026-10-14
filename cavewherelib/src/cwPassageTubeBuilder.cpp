/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeGeometry.h"
#include "cwPassageTubeJunction.h"
#include "cwUnits.h"

//Qt includes
#include <QMap>
#include <QPointF>
#include <QSet>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <numeric>

namespace {

using namespace cwPassageTubeGeometry;

constexpr int kMinFilledBins = 4;
constexpr int kTwistDistributionSamples = 4;
constexpr double kMinSampleWeight = 0.05;
constexpr double kSteepWeight = 0.5;
constexpr double kAlongLegDegrees = 45.0;
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
// A dome rises from its end ring through a ring of half the end's radii,
// at this fraction of its depth, to a point at its full depth: the profile
// of a sphere.
constexpr float kDomeHalfRingScale = 0.5f;
constexpr float kDomeHalfRingDepth = static_cast<float>(std::numbers::sqrt3 / 2.0);
// A wall stretched across a corner's bisector plane reaches at most this many
// times its reading square to the legs.
constexpr double kMaxMiterStretch = 3.0;
constexpr double kCornerInsetMeters = 0.5;
constexpr double kCornerInsetLegFraction = 0.45;
constexpr double kCornerInsetSafety = 1.1;
constexpr double kConcavityClampFactor = 0.95;
constexpr double kClampWarningMeters = 0.1;
// A ring without readings borrows from its own trip within this many legs
// before a nearer ring of another trip.
constexpr int kSameTripBorrowLegs = 2;
constexpr double kInsideBinEpsilon = 1e-4;
// A dead end whose ring is not a simple polygon closes with a fan this far
// from its station toward the ring's centroid: a station against a wall
// leaves thin triangles to that wall when the fan sits on it, and a narrow
// far corner leaves them when the fan sits on the centroid.
constexpr float kFanCenterBlend = 0.5f;
constexpr int kMinLoopStations = 3;
constexpr int kMinDiscVertices = 3;
constexpr double kAxisEpsilon = 1e-9;
constexpr float kMinProjectedLength = 1e-6f;
constexpr float kMinHorizontalLength = 0.01f;
constexpr float kDegenerateLengthSquared = 1e-12f;
constexpr QVector3D kWorldUp(0.0f, 0.0f, 1.0f);
constexpr QVector3D kWorldNorth(0.0f, 1.0f, 0.0f);
constexpr QVector3D kWorldEast(1.0f, 0.0f, 0.0f);

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

// The station's right direction for LRUD read looking along facing.
QVector3D rightOfFacing(const QVector3D& facing)
{
    return QVector3D(facing.y(), -facing.x(), 0.0f);
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
// the neighboring stations is a pit depth or aven height. The ring takes the
// neighbors' reading in its place, or half the width when they have none.
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
                                      const QString& shaftName,
                                      const QString& surfaceName) {
            const std::optional<double> replacement = shaftReplacement(reading, width, median(neighborValues));
            if (!replacement) {
                return;
            }
            if (isFirstVisit) {
                warnings.append(Warning {
                    Warning::ShaftReading,
                    leg.tripId,
                    station,
                    QStringLiteral("Station %1's %2 of %3 m reads as %4; the tube %5 uses %6 m.")
                        .arg(station, readingName)
                        .arg(*reading)
                        .arg(shaftName, surfaceName)
                        .arg(*replacement)
                });
            }
            reading = replacement;
        };
        replaceShaft(passage.up, near.ups, QStringLiteral("up"), QStringLiteral("an aven"), QStringLiteral("ceiling"));
        replaceShaft(passage.down, near.downs, QStringLiteral("down"), QStringLiteral("a pit"), QStringLiteral("floor"));
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

// A station occurrence's LRUD with the facing they were read along.
struct LrudRecord {
    cwStationLrud lrud;
    QVector3D facing;
};

// The unit directions of the two legs a corner station's ring joins.
struct Miter {
    QVector3D incoming;
    QVector3D outgoing;
};

// The corner of a plumb chain start, which no leg ends at.
constexpr int kChainStartCorner = -1;

// The inside of a sharp bend as one of the rings on its legs sees it: bins
// pointing along inside reach at most maxInnerRadius, so the ring stays in
// front of the corner's bisector plane and of its neighbors' planes.
// cornerLeg indexes Chain::legs: the leg that ends at the corner, or
// kChainStartCorner.
struct BendLimit {
    enum Role {
        // A ring along one of the corner's legs.
        Leg,
        // A ring inset from the corner on one of its legs.
        Inset,
        // The corner station's ring, in the bisector plane between the legs in
        // miter, or the middle ring of the elbow that rounds the corner.
        Station,
        // A ring of the elbow that rounds the corner, other than its middle.
        Elbow
    };

    QVector3D inside;
    double maxInnerRadius = 0.0;
    int cornerLeg = 0;
    Role role = Leg;
    Miter miter;
};

// One place along a chain where a ring is fitted. chainLeg indexes
// Chain::legs: the leg the sample lies on or ends. bends holds a limit for
// each sharp bend at either end of the legs the sample lies on. A plumb
// sample is a gentle dead end, whose ring stands in the vertical plane
// through its station. An elbow sample rounds a sharp corner; its middle one
// carries the corner station's records and stationOffset, the station's
// position less its own.
struct ChainSample {
    QVector3D position;
    QVector3D tangent;
    double distanceAlongChain = 0.0;
    QVector<LrudRecord> records;
    int chainLeg = 0;
    bool isStation = false;
    QVector<BendLimit> bends;
    bool isPlumb = false;
    bool isElbow = false;
    QVector3D stationOffset;

    // The limit of the sharp bend at this sample's station, if it has one.
    const BendLimit* stationBend() const
    {
        const auto it = std::find_if(bends.cbegin(), bends.cend(), [](const BendLimit& bend) {
            return bend.role == BendLimit::Station;
        });
        return it != bends.cend() ? &*it : nullptr;
    }
};

LrudRecord lerpRecord(const LrudRecord& a, const LrudRecord& b, double t)
{
    const auto lerp = [t](const std::optional<double>& x, const std::optional<double>& y) -> std::optional<double> {
        if (x && y) {
            return *x + (*y - *x) * t;
        }
        return std::nullopt;
    };
    const QVector3D facing = a.facing + (b.facing - a.facing) * static_cast<float>(t);
    return {
        {lerp(a.lrud.left, b.lrud.left), lerp(a.lrud.right, b.lrud.right), lerp(a.lrud.up, b.lrud.up), lerp(a.lrud.down, b.lrud.down)},
        facing.lengthSquared() < kDegenerateLengthSquared ? a.facing : facing.normalized()
    };
}

// Unit directions of every leg leaving each station, keyed by cave and name.
using StationLegDirections = QHash<StationKey, QVector<QVector3D>>;

StationLegDirections stationLegDirections(const QVector<Leg>& legs)
{
    StationLegDirections directions;
    for (const Leg& leg : legs) {
        const QVector3D direction = (leg.toPosition - leg.fromPosition).normalized();
        directions[{leg.caveId, leg.from}].append(direction);
        directions[{leg.caveId, leg.to}].append(-direction);
    }
    return directions;
}

bool isAlongLeg(const QVector3D& readingDirection, const QVector<QVector3D>& legDirections)
{
    const float minCosine = static_cast<float>(std::cos(qDegreesToRadians(kAlongLegDegrees)));
    return std::any_of(legDirections.cbegin(), legDirections.cend(), [&](const QVector3D& legDirection) {
        return QVector3D::dotProduct(readingDirection, legDirection) > minCosine;
    });
}

bool isSteep(const QVector3D& direction)
{
    return isAlongLeg(kWorldUp, {direction}) || isAlongLeg(-kWorldUp, {direction});
}

// An up or down reading that runs along a leg leaving the station measures
// the pit that leg already surveys, so the ring leaves it out.
cwStationLrud ringLrud(const cwStationLrud& lrud, const QVector<QVector3D>& legDirections)
{
    cwStationLrud ring = lrud;
    if (isAlongLeg(kWorldUp, legDirections)) {
        ring.up.reset();
    }
    if (isAlongLeg(-kWorldUp, legDirections)) {
        ring.down.reset();
    }
    return ring;
}

// A chain leg as walked: start and end in travel order. The ring records
// leave out readings that run along a leg at the station; on a gentle leg the
// other end's floor or ceiling continues to the station.
struct WalkedLeg {
    QVector3D start;
    QVector3D end;
    cwStationLrud startLrud;
    cwStationLrud endLrud;
    LrudRecord startRing;
    LrudRecord endRing;

    QVector3D direction() const { return (end - start).normalized(); }
};

void continueAlongGentleLeg(WalkedLeg& leg)
{
    if (isSteep(leg.direction())) {
        return;
    }
    const auto fillDropped = [](std::optional<double>& ring,
                                const std::optional<double>& measured,
                                const std::optional<double>& otherEnd) {
        if (measured && !ring) {
            ring = otherEnd;
        }
    };
    const cwStationLrud startRing = leg.startRing.lrud;
    const cwStationLrud endRing = leg.endRing.lrud;
    fillDropped(leg.startRing.lrud.up, leg.startLrud.up, endRing.up);
    fillDropped(leg.startRing.lrud.down, leg.startLrud.down, endRing.down);
    fillDropped(leg.endRing.lrud.up, leg.endLrud.up, startRing.up);
    fillDropped(leg.endRing.lrud.down, leg.endLrud.down, startRing.down);
}

WalkedLeg walkedLeg(const QVector<Leg>& legs, const ChainLeg& chainLeg, const StationLegDirections& directions)
{
    const Leg& leg = legs.at(chainLeg.legIndex);
    const LrudRecord fromRing {ringLrud(leg.fromPassageLrud, directions.value({leg.caveId, leg.from})), leg.fromFacing};
    const LrudRecord toRing {ringLrud(leg.toPassageLrud, directions.value({leg.caveId, leg.to})), leg.toFacing};

    WalkedLeg walked = chainLeg.reversed
        ? WalkedLeg {leg.toPosition, leg.fromPosition, leg.toLrud, leg.fromLrud, toRing, fromRing}
        : WalkedLeg {leg.fromPosition, leg.toPosition, leg.fromLrud, leg.toLrud, fromRing, toRing};
    continueAlongGentleLeg(walked);
    return walked;
}

QVector3D bisector(const QVector3D& incoming, const QVector3D& outgoing)
{
    const QVector3D sum = incoming + outgoing;
    if (sum.lengthSquared() < kDegenerateLengthSquared) {
        return incoming;
    }
    return sum.normalized();
}

QVector<WalkedLeg> walkedLegs(const QVector<Leg>& legs, const Chain& chain, const StationLegDirections& directions)
{
    QVector<WalkedLeg> walked;
    walked.reserve(chain.legs.size());
    for (const ChainLeg& chainLeg : chain.legs) {
        walked.append(walkedLeg(legs, chainLeg, directions));
    }
    return walked;
}

bool isLoop(const QVector<WalkedLeg>& walked)
{
    return walked.size() > 1 && walked.first().start == walked.last().end;
}

// Appends sample unless it lies within kMinRingSpacingMeters of the last
// one; a station then takes the place of a nearby mid-shot sample, and two
// nearby stations merge into the first.
void appendSample(QVector<ChainSample>& samples, const ChainSample& sample)
{
    if (samples.isEmpty()
        || samples.last().position.distanceToPoint(sample.position) >= kMinRingSpacingMeters) {
        samples.append(sample);
        return;
    }

    ChainSample& last = samples.last();
    if (sample.isStation) {
        if (last.isStation) {
            last.records.append(sample.records);
        } else {
            last = sample;
        }
    }
}

// The largest passage reading of the records, or fallback when they have none.
double largestReading(const QVector<LrudRecord>& records, double fallback)
{
    std::optional<double> largest;
    for (const LrudRecord& record : records) {
        for (const std::optional<double>& reading : {record.lrud.left, record.lrud.right, record.lrud.up, record.lrud.down}) {
            if (reading) {
                largest = std::max(largest.value_or(*reading), *reading);
            }
        }
    }
    return largest.value_or(fallback);
}

// A bend sharper than Options::subdivideBendDegrees, seen from the corner
// station. A ring square to a leg at distance s from the corner keeps its
// inner wall of radius ρ in front of the bisector plane exactly when
// ρ·tan(φ/2) ≤ s; the station ring, stretched across the bisector plane,
// stays in front of a ring inset by s exactly when its radius ρ_s satisfies
// ρ_s·sin(φ/2) ≤ s.
struct SharpBend {
    Miter miter;
    QVector3D inside;
    double halfTurn = 0.0;
    double largestReading = 0.0;

    // How far the rings would sit from the corner on legs of any length.
    double wantedInset() const
    {
        return kCornerInsetSafety * largestReading * std::tan(halfTurn);
    }

    // How far the rings sit from the corner on a leg of the given length.
    double inset(double legLength) const
    {
        return std::min(std::max(wantedInset(), kCornerInsetMeters), kCornerInsetLegFraction * legLength);
    }

    BendLimit limit(double clearRadius, int cornerLeg, BendLimit::Role role) const
    {
        const double maxInnerRadius = std::max(cwPassageTubeBuilder::kMinRadiusMeters, clearRadius * kConcavityClampFactor);
        return BendLimit {inside, maxInnerRadius, cornerLeg, role, miter};
    }

    BendLimit legLimit(double distance, int cornerLeg, BendLimit::Role role) const
    {
        return limit(distance / std::tan(halfTurn), cornerLeg, role);
    }

    BendLimit stationLimit(double inset, int cornerLeg) const
    {
        return limit(inset / std::sin(halfTurn), cornerLeg, BendLimit::Station);
    }
};

// The direction reflected across the horizontal plane.
QVector3D reflectedVertically(const QVector3D& direction)
{
    return QVector3D(direction.x(), direction.y(), -direction.z());
}

QVector3D horizontalOf(const QVector3D& direction)
{
    return QVector3D(direction.x(), direction.y(), 0.0f).normalized();
}

// A plumb end ring stands in the bisector plane of its leg and that leg
// reflected across the horizontal plane, so it is the corner of a bend
// turning by twice the leg's inclination. The ring before it, square to the
// leg, folds through it when the reading on the bend's inside reaches
// farther than that ring's distance from the end; only then does the end
// need the bend's inset and inner-wall limits.
std::optional<SharpBend> plumbEndBend(const Miter& miter, const LrudRecord& record, double firstSampleDistance, const Options& options)
{
    const QVector3D turn = miter.outgoing - miter.incoming;
    if (turn.lengthSquared() < kDegenerateLengthSquared) {
        return std::nullopt;
    }
    const QVector3D inside = turn.normalized();
    const double halfTurn = std::asin(std::min(1.0, static_cast<double>(std::abs(miter.incoming.z()))));
    const std::optional<double> insideReading = inside.z() > 0.0f ? record.lrud.up : record.lrud.down;
    const double reading = insideReading.value_or(largestReading({record}, options.defaultRadiusMeters));
    if (kCornerInsetSafety * reading * std::sin(halfTurn) <= firstSampleDistance) {
        return std::nullopt;
    }
    return SharpBend {miter, inside, halfTurn, reading};
}

QVector3D rotateAbout(const QVector3D& axis, const QVector3D& v, double angle)
{
    const float c = static_cast<float>(std::cos(angle));
    const float s = static_cast<float>(std::sin(angle));
    return v * c + QVector3D::crossProduct(axis, v) * s + axis * QVector3D::dotProduct(axis, v) * (1.0f - c);
}

// Which ends of an open chain get a plumb ring, and how far past its
// station an end closed by a dome, or a through that ends past its
// junction, carries one more ring, and whether that ring is plumb; a loop
// has neither.
struct ChainEnds {
    bool isStartPlumb = false;
    bool isEndPlumb = false;
    double startOffset = 0.0;
    double endOffset = 0.0;
    bool isStartOffsetPlumb = false;
    bool isEndOffsetPlumb = false;
};

QVector<ChainSample> chainSamples(const QVector<WalkedLeg>& walked, const ChainEnds& ends, const Options& options)
{
    const bool loop = isLoop(walked);
    const double spacing = std::max(options.sampleSpacingMeters, kMinSampleSpacingMeters);
    const float minBendCosine = static_cast<float>(std::cos(qDegreesToRadians(options.subdivideBendDegrees)));
    const int legCount = walked.size();
    const int lastLeg = legCount - 1;

    QVector<double> lengths;
    lengths.reserve(legCount);
    for (const WalkedLeg& leg : walked) {
        lengths.append(leg.start.distanceToPoint(leg.end));
    }
    const auto intervalsOf = [&](int legIndex) {
        return std::max(1, static_cast<int>(std::lround(lengths.at(legIndex) / spacing)));
    };
    const auto firstSampleDistance = [&](int legIndex) {
        return lengths.at(legIndex) / intervalsOf(legIndex);
    };

    const auto sharpBendAfter = [&](int legIndex) -> std::optional<SharpBend> {
        const WalkedLeg& leg = walked.at(legIndex);
        const QVector3D incoming = leg.direction();
        const bool hasNext = loop || legIndex < lastLeg;
        if (!hasNext) {
            if (ends.isEndPlumb) {
                return plumbEndBend({incoming, reflectedVertically(incoming)}, leg.endRing, firstSampleDistance(legIndex), options);
            }
            return std::nullopt;
        }
        const int nextIndex = (legIndex + 1) % legCount;
        const WalkedLeg& next = walked.at(nextIndex);
        const QVector3D outgoing = next.direction();
        const float cosine = QVector3D::dotProduct(incoming, outgoing);
        const double turn = std::acos(std::clamp(static_cast<double>(cosine), -1.0, 1.0));
        const SharpBend bend {
            {incoming, outgoing},
            (outgoing - incoming).normalized(),
            turn / 2.0,
            largestReading({leg.endRing, next.startRing}, options.defaultRadiusMeters)
        };
        // A gentler bend is sharp too when its walls reach past the rings
        // beside the corner.
        const double nearestRing = std::min(firstSampleDistance(legIndex), firstSampleDistance(nextIndex));
        if (cosine >= minBendCosine && bend.wantedInset() <= nearestRing) {
            return std::nullopt;
        }
        return bend;
    };

    QVector<std::optional<SharpBend>> bendAfter;
    bendAfter.reserve(legCount);
    for (int legIndex = 0; legIndex < legCount; ++legIndex) {
        bendAfter.append(sharpBendAfter(legIndex));
    }
    std::optional<SharpBend> startBend;
    if (ends.isStartPlumb) {
        const QVector3D outgoing = walked.first().direction();
        startBend = plumbEndBend({reflectedVertically(outgoing), outgoing}, walked.first().startRing, firstSampleDistance(0), options);
    }

    // A sharp corner, named by the index of the leg ending there, or
    // kChainStartCorner for a plumb start. On a loop the first leg starts at
    // the last leg's corner.
    const auto bendAt = [&](int corner) -> const SharpBend& {
        return corner == kChainStartCorner ? *startBend : *bendAfter.at(corner);
    };
    const auto endCorner = [&](int legIndex) -> std::optional<int> {
        if (bendAfter.at(legIndex)) {
            return legIndex;
        }
        return std::nullopt;
    };
    const auto startCorner = [&](int legIndex) -> std::optional<int> {
        if (legIndex > 0 || loop) {
            return endCorner(legIndex > 0 ? legIndex - 1 : lastLeg);
        }
        if (startBend) {
            return kChainStartCorner;
        }
        return std::nullopt;
    };
    const auto legLimit = [&](int corner, double distance, BendLimit::Role role) {
        return bendAt(corner).legLimit(distance, corner, role);
    };

    // The limits of the bends at the far ends of legIn and legOut for a ring
    // nearer than their full length to those bends.
    const auto farBends = [&](int legIn, int legOut, double nearer) {
        QVector<BendLimit> bends;
        if (legIn >= 0) {
            if (const std::optional<int> corner = startCorner(legIn)) {
                bends.append(legLimit(*corner, lengths.at(legIn) - nearer, BendLimit::Leg));
            }
        }
        if (legOut >= 0) {
            if (const std::optional<int> corner = endCorner(legOut)) {
                bends.append(legLimit(*corner, lengths.at(legOut) - nearer, BendLimit::Leg));
            }
        }
        return bends;
    };

    // The inset of the corner between the two legs, either of which is -1
    // at a dead end: the nearer of the two legs' insets.
    const auto cornerInset = [&](int corner, int legIn, int legOut) {
        double inset = std::numeric_limits<double>::max();
        for (int leg : {legIn, legOut}) {
            if (leg >= 0) {
                inset = std::min(inset, bendAt(corner).inset(lengths.at(leg)));
            }
        }
        return inset;
    };

    // A station ends legIn and starts legOut; either is -1 at a dead end.
    // Its ring keeps clear of the bends at the far ends of both legs, and
    // a corner station's ring of both inset rings.
    const auto stationBends = [&](int legIn, int legOut) {
        QVector<BendLimit> bends;
        if (const std::optional<int> corner = legIn >= 0 ? endCorner(legIn) : startCorner(legOut)) {
            bends.append(bendAt(*corner).stationLimit(cornerInset(*corner, legIn, legOut), *corner));
        }
        bends.append(farBends(legIn, legOut, 0.0));
        return bends;
    };

    QVector<ChainSample> samples;

    // The elbow rounding the sharp corner at the end of legIndex, whose
    // station sample is station: rings on the arc tangent to both legs at the
    // corner's inset, turning by at most kElbowStepDegrees from one to the
    // next. There is an odd number of them so the middle one faces along the
    // bisector and carries the station's records; the others take no records
    // of their own (see blendElbowRings). A corner turning less than one step
    // keeps the single station ring.
    const auto elbowSamples = [&](int legIndex, const ChainSample& station) {
        const SharpBend& bend = *bendAfter.at(legIndex);
        const int nextLeg = (legIndex + 1) % legCount;
        const double turn = 2.0 * bend.halfTurn;
        int count = static_cast<int>(std::ceil(qRadiansToDegrees(turn) / kElbowStepDegrees));
        if (count % 2 == 0) {
            ++count;
        }
        const QVector3D axis = QVector3D::crossProduct(bend.miter.incoming, bend.miter.outgoing);
        if (count == 1 || axis.length() < kMinProjectedLength) {
            return QVector<ChainSample> {station};
        }

        const double inset = cornerInset(legIndex, legIndex, nextLeg);
        const double radius = inset / std::tan(bend.halfTurn);
        const QVector3D center = station.position + bend.inside * static_cast<float>(inset / std::sin(bend.halfTurn));
        const QVector3D arcStart = station.position - bend.miter.incoming * static_cast<float>(inset) - center;
        const QVector3D unitAxis = axis.normalized();
        const QVector<BendLimit> far = farBends(legIndex, nextLeg, inset);

        QVector<ChainSample> elbow;
        elbow.reserve(count);
        const int middle = count / 2;
        for (int j = 0; j < count; ++j) {
            const double angle = (j + 0.5) / count * turn;
            const bool isMiddle = j == middle;
            ChainSample sample = isMiddle ? station : ChainSample {{}, {}, 0.0, {}, j < middle ? legIndex : nextLeg};
            sample.position = center + rotateAbout(unitAxis, arcStart, angle);
            sample.tangent = rotateAbout(unitAxis, bend.miter.incoming, angle).normalized();
            sample.distanceAlongChain = station.distanceAlongChain + radius * (angle - bend.halfTurn);
            sample.bends = {bend.limit(radius, legIndex, isMiddle ? BendLimit::Station : BendLimit::Elbow)};
            sample.bends.append(far);
            sample.isElbow = true;
            if (isMiddle) {
                sample.stationOffset = station.position - sample.position;
            }
            elbow.append(sample);
        }
        return elbow;
    };

    // Appends the elbow's rings from first to last, inclusive.
    const auto appendElbow = [&](const QVector<ChainSample>& elbow, int first, int last) {
        for (int j = first; j <= last; ++j) {
            appendSample(samples, elbow.at(j));
        }
    };

    // A loop starts and ends on its seam station's ring. A sharp seam opens
    // with the second half of its elbow and closes with the first half.
    const bool hasSharpSeam = loop && endCorner(lastLeg).has_value();

    double distance = 0.0;
    for (int legIndex = 0; legIndex < legCount; ++legIndex) {
        const WalkedLeg& leg = walked.at(legIndex);
        const QVector3D direction = leg.direction();
        const double length = lengths.at(legIndex);

        if (legIndex == 0) {
            QVector3D tangent = direction;
            if (loop) {
                tangent = bisector(walked.last().direction(), direction);
            } else if (ends.isStartPlumb) {
                tangent = horizontalOf(direction);
            }
            const ChainSample start {leg.start, tangent, 0.0, {leg.startRing}, legIndex, true,
                                     stationBends(loop ? lastLeg : -1, legIndex), ends.isStartPlumb};
            if (ends.startOffset >= kMinRingSpacingMeters) {
                const double offset = ends.startOffset;
                const QVector3D offsetTangent = ends.isStartOffsetPlumb ? horizontalOf(direction) : direction;
                samples.append(ChainSample {leg.start - direction * static_cast<float>(offset), offsetTangent, -offset,
                                            {leg.startRing}, legIndex, false, farBends(-1, legIndex, -offset), ends.isStartOffsetPlumb});
            }
            if (hasSharpSeam) {
                const QVector<ChainSample> seam = elbowSamples(lastLeg, start);
                appendElbow(seam, seam.size() / 2, seam.size() - 1);
            } else {
                appendSample(samples, start);
            }
        }

        const std::optional<int> startCornerIndex = startCorner(legIndex);
        const std::optional<int> endCornerIndex = endCorner(legIndex);
        const double startInset = startCornerIndex ? bendAt(*startCornerIndex).inset(length) : 0.0;
        const double endInset = endCornerIndex ? bendAt(*endCornerIndex).inset(length) : 0.0;
        const double startInsetFraction = startInset / length;
        const double endInsetFraction = 1.0 - endInset / length;

        const auto appendAlongLeg = [&](double t, BendLimit::Role startRole, BendLimit::Role endRole) {
            QVector<BendLimit> bends;
            if (startCornerIndex) {
                bends.append(legLimit(*startCornerIndex, t * length, startRole));
            }
            if (endCornerIndex) {
                bends.append(legLimit(*endCornerIndex, (1.0 - t) * length, endRole));
            }
            appendSample(samples, ChainSample {
                leg.start + (leg.end - leg.start) * static_cast<float>(t),
                direction,
                distance + length * t,
                {lerpRecord(leg.startRing, leg.endRing, t)},
                legIndex,
                false,
                bends
            });
        };

        if (startCornerIndex) {
            appendAlongLeg(startInsetFraction, BendLimit::Inset, BendLimit::Leg);
        }
        const int intervals = intervalsOf(legIndex);
        for (int i = 1; i < intervals; ++i) {
            const double t = static_cast<double>(i) / intervals;
            const bool isPastStartInset = !startCornerIndex || t > startInsetFraction;
            const bool isBeforeEndInset = !endCornerIndex || t < endInsetFraction;
            if (isPastStartInset && isBeforeEndInset) {
                appendAlongLeg(t, BendLimit::Leg, BendLimit::Leg);
            }
        }
        if (endCornerIndex) {
            appendAlongLeg(endInsetFraction, BendLimit::Leg, BendLimit::Inset);
        }

        distance += length;
        const bool isLast = legIndex == lastLeg;
        const bool isPlumb = isLast && ends.isEndPlumb;
        QVector3D endTangent = isPlumb ? horizontalOf(direction) : direction;
        QVector<LrudRecord> endRecords {leg.endRing};
        if (isLast) {
            if (loop) {
                endTangent = samples.first().tangent;
            }
        } else {
            const WalkedLeg& next = walked.at(legIndex + 1);
            endTangent = bisector(direction, next.direction());
            endRecords.append(next.startRing);
        }
        const int legOut = isLast ? (loop ? 0 : -1) : legIndex + 1;
        const ChainSample station {leg.end, endTangent, distance, endRecords, legIndex, true,
                                   stationBends(legIndex, legOut), isPlumb};
        if (endCornerIndex && (!isLast || hasSharpSeam)) {
            const QVector<ChainSample> elbow = elbowSamples(legIndex, station);
            appendElbow(elbow, 0, isLast ? elbow.size() / 2 : elbow.size() - 1);
        } else {
            appendSample(samples, station);
        }
        if (isLast && ends.endOffset >= kMinRingSpacingMeters) {
            const double offset = ends.endOffset;
            const QVector3D offsetTangent = ends.isEndOffsetPlumb ? horizontalOf(direction) : direction;
            samples.append(ChainSample {leg.end + direction * static_cast<float>(offset), offsetTangent, distance + offset,
                                        {leg.endRing}, legIndex, false, farBends(legIndex, -1, -offset), ends.isEndOffsetPlumb});
        }
    }
    return samples;
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

// v projected into the plane perpendicular to the unit vector normal, or
// nullopt when v is (nearly) parallel to it.
std::optional<QVector3D> projectedInPlane(const QVector3D& v, const QVector3D& normal)
{
    const QVector3D projected = v - QVector3D::dotProduct(v, normal) * normal;
    if (projected.length() < kMinProjectedLength) {
        return std::nullopt;
    }
    return projected.normalized();
}

double signedAngle(const QVector3D& from, const QVector3D& to, const QVector3D& axis)
{
    return std::atan2(QVector3D::dotProduct(QVector3D::crossProduct(from, to), axis),
                      QVector3D::dotProduct(from, to));
}

// The angle + 2πn closest to reference.
double unwrapped(double angle, double reference)
{
    return angle + kTwoPi * std::round((reference - angle) / kTwoPi);
}

// Wang et al. 2008, double reflection: carries the up vector from the
// previous sample to this one with minimal rotation about the tangent.
QVector3D doubleReflection(const QVector3D& previousUp,
                           const QVector3D& previousTangent,
                           const QVector3D& previousPosition,
                           const QVector3D& position,
                           const QVector3D& tangent)
{
    QVector3D reflectedUp = previousUp;
    QVector3D reflectedTangent = previousTangent;
    const QVector3D v1 = position - previousPosition;
    const float c1 = QVector3D::dotProduct(v1, v1);
    if (c1 > kDegenerateLengthSquared) {
        reflectedUp = previousUp - (2.0f / c1) * QVector3D::dotProduct(v1, previousUp) * v1;
        reflectedTangent = previousTangent - (2.0f / c1) * QVector3D::dotProduct(v1, previousTangent) * v1;
    }

    QVector3D up = reflectedUp;
    const QVector3D v2 = tangent - reflectedTangent;
    const float c2 = QVector3D::dotProduct(v2, v2);
    if (c2 > kDegenerateLengthSquared) {
        up = reflectedUp - (2.0f / c2) * QVector3D::dotProduct(v2, reflectedUp) * v2;
    }
    return projectedInPlane(up, tangent).value_or(previousUp);
}

Frame makeFrame(const QVector3D& tangent, const QVector3D& up, double weight)
{
    return Frame {tangent, up, QVector3D::crossProduct(tangent, up), weight};
}

QVector<Frame> chainFrames(const QVector<ChainSample>& samples, const Options& options)
{
    const int count = samples.size();
    QVector<double> weights;
    weights.reserve(count);
    for (const ChainSample& sample : samples) {
        weights.append(inclinationWeight(sample.tangent, options));
    }

    QVector<Frame> frames(count);
    const auto rmfFrom = [&](int previous, int index) {
        return doubleReflection(frames.at(previous).up,
                                frames.at(previous).tangent,
                                samples.at(previous).position,
                                samples.at(index).position,
                                samples.at(index).tangent);
    };

    // Seed at the first sample short of vertical; a chain vertical
    // throughout seeds from world +Y.
    const auto firstGentle = std::find_if(weights.cbegin(), weights.cend(), [](double w) { return w < 1.0; });
    const bool isVertical = firstGentle == weights.cend();
    const int seed = isVertical ? 0 : static_cast<int>(firstGentle - weights.cbegin());
    const QVector3D seedTangent = samples.at(seed).tangent;
    const QVector3D seedUp = projectedInPlane(isVertical ? kWorldNorth : kWorldUp, seedTangent)
                                 .value_or(projectedInPlane(kWorldEast, seedTangent).value_or(kWorldNorth));
    frames[seed] = makeFrame(seedTangent, seedUp, weights.at(seed));

    // Samples before the seed are steep, so they follow the transported frame.
    for (int i = seed - 1; i >= 0; --i) {
        frames[i] = makeFrame(samples.at(i).tangent, rmfFrom(i + 1, i), weights.at(i));
    }

    double previousAngle = 0.0;
    bool inSteepRun = weights.at(seed) > kSteepWeight;
    double residualTwist = 0.0;
    int twistStep = kTwistDistributionSamples;
    for (int i = seed + 1; i < count; ++i) {
        const QVector3D tangent = samples.at(i).tangent;
        const double weight = weights.at(i);
        const QVector3D rmfUp = rmfFrom(i - 1, i);
        const QVector3D horizontalUp = projectedInPlane(kWorldUp, tangent)
                                           .value_or(projectedInPlane(frames.at(i - 1).up, tangent).value_or(rmfUp));

        const double angle = unwrapped(signedAngle(horizontalUp, rmfUp, tangent), previousAngle);
        double appliedAngle = weight * angle;

        if (weight > kSteepWeight) {
            inSteepRun = true;
            twistStep = kTwistDistributionSamples;
        } else if (inSteepRun) {
            inSteepRun = false;
            residualTwist = angle;
            twistStep = 0;
        }

        // Leaving a pit, unwind the twist the transported frame picked up
        // over the next few samples instead of all at once.
        if (twistStep < kTwistDistributionSamples) {
            ++twistStep;
            const double remaining = 1.0 - static_cast<double>(twistStep) / kTwistDistributionSamples;
            appliedAngle += (1.0 - weight) * residualTwist * remaining;
        }

        frames[i] = makeFrame(tangent, rotateAbout(tangent, horizontalUp, appliedAngle).normalized(), weight);
        previousAngle = angle;
    }

    for (int i = 0; i < count; ++i) {
        if (samples.at(i).isPlumb) {
            frames[i] = makeFrame(samples.at(i).tangent, kWorldUp, 0.0);
        }
    }
    return frames;
}

// The record's walls as seen across the ring: each wall foreshortened to its
// component along frame.right or frame.up, the side walls swapped when the
// record faces the other way. A record facing along the ring's tangent keeps
// only up and down.
//
// On a corner station's ring each wall instead slides along the leg whose
// cross-section holds it onto the bisector plane and reaches as far from
// the station as it lands, so a wall read square to one leg lands where that
// leg's wall meets the ring. On a sloped corner the slide also carries a
// vertical wall sideways, off the ring's up axis.
cwStationLrud lrudAcross(const LrudRecord& record, const Frame& frame, const std::optional<Miter>& miter)
{
    const auto acrossRing = [&](const std::optional<double>& reading, const QVector3D& direction, const QVector3D& axis) -> std::optional<double> {
        if (!reading) {
            return std::nullopt;
        }
        QVector3D wall = direction * static_cast<float>(*reading);
        if (miter) {
            const bool isAcrossIncoming = std::abs(QVector3D::dotProduct(direction, miter->incoming))
                                          <= std::abs(QVector3D::dotProduct(direction, miter->outgoing));
            const QVector3D& leg = isAcrossIncoming ? miter->incoming : miter->outgoing;
            wall -= leg * (QVector3D::dotProduct(wall, frame.tangent) / QVector3D::dotProduct(leg, frame.tangent));
            const double stretched = std::min(static_cast<double>(wall.length()), kMaxMiterStretch * *reading);
            return std::max(cwPassageTubeBuilder::kMinRadiusMeters, stretched);
        }
        return std::max(cwPassageTubeBuilder::kMinRadiusMeters,
                        static_cast<double>(std::abs(QVector3D::dotProduct(wall, axis))));
    };

    cwStationLrud across {std::nullopt,
                          std::nullopt,
                          acrossRing(record.lrud.up, kWorldUp, frame.up),
                          acrossRing(record.lrud.down, -kWorldUp, frame.up)};
    const QVector3D right = rightOfFacing(record.facing);
    const float component = QVector3D::dotProduct(right, frame.right);
    if (std::abs(component) < kMinProjectedLength) {
        return across;
    }
    const std::optional<double> rightWall = acrossRing(record.lrud.right, right, frame.right);
    const std::optional<double> leftWall = acrossRing(record.lrud.left, -right, frame.right);
    const bool facesSameWay = component >= 0.0f;
    across.right = facesSameWay ? rightWall : leftWall;
    across.left = facesSameWay ? leftWall : rightWall;
    return across;
}

// stationRadii are the radii of the station's section around the station
// itself, which a ring without readings borrows; as fitted, they differ from
// radii only on an elbow's middle ring, until fitSharpBends reshapes the bins
// that point into a bend. section is set only on a fit from the quadrant
// readings.
struct RingFit {
    QVector<double> radii;
    QVector<double> binAngles;
    QVector<bool> measured;
    int filledBins = 0;
    QVector<double> stationRadii;
    std::optional<Section> section;
};

// Bin k's eccentric anomaly around the ring, 2πk/N.
double binParameter(int bin, int resolution)
{
    return kTwoPi * bin / resolution;
}

RingFit emptyFit(int resolution)
{
    QVector<double> roundAngles;
    roundAngles.reserve(resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        roundAngles.append(binParameter(bin, resolution));
    }

    const QVector<double> zeros(resolution, 0.0);
    return RingFit {zeros, roundAngles, QVector<bool>(resolution, false), 0, zeros, std::nullopt};
}

// A pit's left and right read across it whichever way it is entered, so a
// steep ring is round at their mean, or at the one that was read.
RingFit fitSteepRing(const QVector<LrudRecord>& records, int resolution)
{
    std::optional<double> radius;
    for (const LrudRecord& record : records) {
        const std::optional<double>& left = record.lrud.left;
        const std::optional<double>& right = record.lrud.right;
        if (left || right) {
            const double recordRadius = left && right ? (*left + *right) / 2.0 : left.value_or(right.value_or(0.0));
            radius = std::max(radius.value_or(recordRadius), recordRadius);
        }
    }

    RingFit fit = emptyFit(resolution);
    if (radius) {
        fit.radii.fill(*radius);
        fit.stationRadii = fit.radii;
        fit.measured.fill(true);
        fit.filledBins = resolution;
    }
    return fit;
}

// The widest reading of each wall over the records.
cwStationLrud widestReadings(const QVector<cwStationLrud>& readings)
{
    const auto widen = [](std::optional<double>& widest, const std::optional<double>& reading) {
        if (reading) {
            widest = std::max(widest.value_or(*reading), *reading);
        }
    };
    cwStationLrud widest;
    for (const cwStationLrud& lrud : readings) {
        widen(widest.left, lrud.left);
        widen(widest.right, lrud.right);
        widen(widest.up, lrud.up);
        widen(widest.down, lrud.down);
    }
    return widest;
}

// A missing wall takes the mean of the walls beside it around the ring: a
// side wall of up and down, a ceiling or floor of left and right.
cwStationLrud filledReadings(const cwStationLrud& lrud)
{
    const auto meanOf = [](const std::optional<double>& a, const std::optional<double>& b) -> std::optional<double> {
        if (a && b) {
            return (*a + *b) / 2.0;
        }
        return a ? a : b;
    };
    return {
        lrud.left ? lrud.left : meanOf(lrud.up, lrud.down),
        lrud.right ? lrud.right : meanOf(lrud.up, lrud.down),
        lrud.up ? lrud.up : meanOf(lrud.left, lrud.right),
        lrud.down ? lrud.down : meanOf(lrud.left, lrud.right)
    };
}

// The quadrant's horizontal and vertical readings at eccentric anomaly t.
std::pair<std::optional<double>, std::optional<double>> quadrantReadings(const cwStationLrud& lrud, double t)
{
    return {std::cos(t) >= 0.0 ? lrud.right : lrud.left, std::sin(t) >= 0.0 ? lrud.up : lrud.down};
}

// The walls with those on center's side of the station grown so their
// quadrant ellipse holds center, a point given as (right, up) from the
// station, at least kMinRadiusMeters inside it: first each axis past
// center's reach along it, then both together along center's direction.
cwStationLrud enclosingCenter(const cwStationLrud& lrud, const QPointF& center)
{
    constexpr double kMinRadius = cwPassageTubeBuilder::kMinRadiusMeters;
    cwStationLrud grown = lrud;
    std::optional<double>& horizontal = center.x() >= 0.0 ? grown.right : grown.left;
    std::optional<double>& vertical = center.y() >= 0.0 ? grown.up : grown.down;
    horizontal = std::max(horizontal.value_or(kMinRadius), std::abs(center.x()) + kMinRadius);
    vertical = std::max(vertical.value_or(kMinRadius), std::abs(center.y()) + kMinRadius);

    const double distance = std::hypot(center.x(), center.y());
    if (distance > 0.0) {
        const double a = *horizontal;
        const double b = *vertical;
        const double reach = a * b * distance / std::hypot(b * center.x(), a * center.y());
        const double scale = (distance + kMinRadius) / reach;
        if (scale > 1.0) {
            horizontal = a * scale;
            vertical = b * scale;
        }
    }
    return grown;
}

// How far the ray from center along angle runs to the quadrant ellipse of
// lrud around the origin, with center inside it: the root of the one
// quadrant's ellipse that the ray meets inside that quadrant.
double reachFromPoint(const cwStationLrud& lrud, const QPointF& center, double angle)
{
    constexpr double kMinRadius = cwPassageTubeBuilder::kMinRadiusMeters;
    const QPointF direction(std::cos(angle), std::sin(angle));
    std::optional<double> reach;
    for (const double horizontalSign : {1.0, -1.0}) {
        for (const double verticalSign : {1.0, -1.0}) {
            const double a = (horizontalSign > 0.0 ? lrud.right : lrud.left).value_or(kMinRadius);
            const double b = (verticalSign > 0.0 ? lrud.up : lrud.down).value_or(kMinRadius);
            const double quadratic = std::pow(direction.x() / a, 2) + std::pow(direction.y() / b, 2);
            const double linear = 2.0 * (center.x() * direction.x() / (a * a) + center.y() * direction.y() / (b * b));
            const double constant = std::pow(center.x() / a, 2) + std::pow(center.y() / b, 2) - 1.0;
            const double discriminant = linear * linear - 4.0 * quadratic * constant;
            if (discriminant < 0.0) {
                continue;
            }
            const double root = (-linear + std::sqrt(discriminant)) / (2.0 * quadratic);
            const QPointF hit = center + direction * root;
            const bool isInQuadrant = hit.x() * horizontalSign >= -kAxisEpsilon && hit.y() * verticalSign >= -kAxisEpsilon;
            if (root > 0.0 && isInQuadrant) {
                reach = std::min(reach.value_or(root), root);
            }
        }
    }
    return reach.value_or(kMinRadius);
}

// The section a ring fits to its station's walls, in the ring's (right, up)
// plane around its center. An elbow's middle ring sits off its station
// toward the bend's inside, so its section is the walls' quadrant ellipse
// around its center joined with the same ellipse around the station: the
// walls on the bend's outside stay where they were read, and the station
// lies inside by at least its reading there.
class LrudSection
{
public:
    LrudSection(const cwStationLrud& walls, const Frame& frame, const QVector3D& stationOffset) :
        m_walls(walls),
        m_station(QVector3D::dotProduct(stationOffset, frame.right), QVector3D::dotProduct(stationOffset, frame.up)),
        m_wallsAroundStation(enclosingCenter(walls, -m_station))
    {
    }

    double reach(double angle) const
    {
        return std::max(reachAroundStation(angle), reachFromPoint(m_wallsAroundStation, -m_station, angle));
    }

    // The section's reach if the ring were centered on its station.
    double reachAroundStation(double angle) const
    {
        return reachFromPoint(m_walls, QPointF(), angle);
    }

private:
    cwStationLrud m_walls;
    QPointF m_station;
    cwStationLrud m_wallsAroundStation;
};

// Fits the ring to the LRUD quadrant ellipse. Bin k sits at eccentric anomaly
// t_k = 2πk/N, the vertex (a cos t_k, b sin t_k) for the quadrant's
// horizontal and vertical readings a and b, so the bins spread along a thin
// ellipse's walls instead of bunching at its sides. A bin is measured when
// the readings its quadrant needs were read; a missing reading is filled from
// the walls beside it. Bins are left at zero when fewer than kMinFilledBins
// are measured. Each bin reaches to the LrudSection along its angle.
RingFit fitLrudRing(const ChainSample& sample, const Frame& frame, int resolution)
{
    if (1.0 - frame.inclinationWeight < kMinSampleWeight) {
        return fitSteepRing(sample.records, resolution);
    }

    const BendLimit* stationBend = sample.stationBend();
    const std::optional<Miter> miter = stationBend ? std::optional<Miter>(stationBend->miter) : std::nullopt;
    QVector<cwStationLrud> across;
    across.reserve(sample.records.size());
    for (const LrudRecord& record : sample.records) {
        across.append(lrudAcross(record, frame, miter));
    }
    const cwStationLrud read = widestReadings(across);

    RingFit fit = emptyFit(resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        const double t = binParameter(bin, resolution);
        const bool usesHorizontal = std::abs(std::cos(t)) > kAxisEpsilon;
        const bool usesVertical = std::abs(std::sin(t)) > kAxisEpsilon;
        const auto [a, b] = quadrantReadings(read, t);
        fit.measured[bin] = (!usesHorizontal || a) && (!usesVertical || b);
    }
    fit.filledBins = static_cast<int>(std::count(fit.measured.cbegin(), fit.measured.cend(), true));
    if (fit.filledBins < kMinFilledBins) {
        return fit;
    }

    const cwStationLrud filled = filledReadings(read);
    const LrudSection section(filled, frame, sample.stationOffset);
    for (int bin = 0; bin < resolution; ++bin) {
        const double t = binParameter(bin, resolution);
        const auto [a, b] = quadrantReadings(filled, t);
        const double x = a.value_or(cwPassageTubeBuilder::kMinRadiusMeters) * std::cos(t);
        const double y = b.value_or(cwPassageTubeBuilder::kMinRadiusMeters) * std::sin(t);
        const double angle = std::atan2(y, x);
        fit.binAngles[bin] = angle < 0.0 ? angle + kTwoPi : angle;
        fit.radii[bin] = section.reach(angle);
        fit.stationRadii[bin] = section.reachAroundStation(angle);
    }
    fit.section = Section {filled, fit.stationRadii, sample.position + sample.stationOffset};
    return fit;
}

// A ring without data takes the nearest fitted ring. In a pit that ring's
// ceiling and floor would land on horizontal axes through the rolled frame,
// so the pit is round at the fitted ring's horizontal half-width instead.
RingFit borrowedFit(const RingFit& fitted, const Frame& frame)
{
    const QVector<double>& radii = fitted.stationRadii;
    if (frame.inclinationWeight <= kSteepWeight) {
        RingFit borrowed = fitted;
        borrowed.radii = radii;
        return borrowed;
    }
    const int resolution = radii.size();
    const int rightBin = 0;
    const int leftBin = resolution / 2;
    RingFit round = emptyFit(resolution);
    round.radii.fill((radii.at(rightBin) + radii.at(leftBin)) / 2.0);
    return round;
}

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

using EndCap = cwPassageTubeBuilder::EndCap;
using MouthPoint = cwPassageTubeBuilder::MouthPoint;

double halfTurnCosine(const Ring& ring, const BendLimit& bend)
{
    return QVector3D::dotProduct(bend.miter.incoming, ring.frame.tangent);
}

// The radius of a section square to the legs over its radius stretched
// across the bisector plane, for a bin whose component along the bend's
// inside is inside, at least 1 / kMaxMiterStretch.
double squareOverMiterRadius(double halfTurnCosine, double inside)
{
    const double halfTurnSineSquared = 1.0 - halfTurnCosine * halfTurnCosine;
    return std::max(std::sqrt(1.0 - inside * inside * halfTurnSineSquared), 1.0 / kMaxMiterStretch);
}

// A corner station's ring without readings of its own takes a section
// square to the legs, stretched across the bisector plane to meet their walls.
void stretchAcrossMiter(Ring& ring, const BendLimit& bend)
{
    const double cosine = halfTurnCosine(ring, bend);
    const int resolution = ring.radii.size();
    for (int bin = 0; bin < resolution; ++bin) {
        const double inside = QVector3D::dotProduct(binDirection(ring, bin), bend.inside);
        ring.radii[bin] /= squareOverMiterRadius(cosine, inside);
    }
}

// A borrowed section on a ring stationOffset off its station, as
// LrudSection joins it: the bins toward the station reach as far past it as
// the section does, so the station lies inside.
void reachPastStation(Ring& ring, const QVector3D& stationOffset)
{
    for (int bin = 0; bin < ring.radii.size(); ++bin) {
        ring.radii[bin] += std::max(0.0f, QVector3D::dotProduct(stationOffset, binDirection(ring, bin)));
    }
}

// How far the ring's measured walls lie past the station's readings. An
// elbow's middle ring reads the walls on the bend's inside from its center,
// which sits stationOffset nearer the inside than the station; the walls on
// the outside stay where they were read.
double insideWallShift(const Ring& ring, const QVector3D& stationOffset)
{
    double shift = 0.0;
    for (int bin = 0; bin < ring.radii.size(); ++bin) {
        if (ring.measured.at(bin)) {
            shift = std::max(shift, static_cast<double>(-QVector3D::dotProduct(stationOffset, binDirection(ring, bin))));
        }
    }
    return shift;
}

// How fitting a ring to its bend changed it: whether any bin moved, and how
// far the most-moved measured bin went.
struct BendChange {
    bool changed = false;
    double largestMeasuredChange = 0.0;

    void add(const BendChange& other)
    {
        changed = changed || other.changed;
        largestMeasuredChange = std::max(largestMeasuredChange, other.largestMeasuredChange);
    }
};

// Each elbow ring other than the middle one takes the section blended between
// the rings either side of its run of such rings, by distance along the
// chain: the ring before the elbow and its middle ring, or the middle ring and
// the ring after. The section turns smoothly from the passage's into the
// station's, though the two read their walls facing different ways.
void blendElbowRings(const QVector<ChainSample>& samples, Chain& chain)
{
    const int count = samples.size();
    const auto isBlended = [&](int i) {
        return samples.at(i).isElbow && !samples.at(i).isStation;
    };
    for (int i = 0; i < count; ++i) {
        if (!isBlended(i)) {
            continue;
        }
        int before = i - 1;
        while (before >= 0 && isBlended(before)) {
            --before;
        }
        int after = i + 1;
        while (after < count && isBlended(after)) {
            ++after;
        }
        if (before < 0 || after >= count) {
            continue;
        }
        const double span = samples.at(after).distanceAlongChain - samples.at(before).distanceAlongChain;
        const double fraction = span > 0.0 ? (samples.at(i).distanceAlongChain - samples.at(before).distanceAlongChain) / span : 0.5;
        const Ring& low = chain.rings.at(before);
        const Ring& high = chain.rings.at(after);
        Ring& ring = chain.rings[i];
        ring.section.reset();
        for (int bin = 0; bin < ring.radii.size(); ++bin) {
            ring.radii[bin] = low.radii.at(bin) + (high.radii.at(bin) - low.radii.at(bin)) * fraction;
            ring.binAngles[bin] = low.binAngles.at(bin) + (high.binAngles.at(bin) - low.binAngles.at(bin)) * fraction;
            ring.measured[bin] = false;
        }
    }
}

// Sets each bin of the ring that points into the bend to radiusAt(bin, its
// current radius), marking the bins that move unmeasured.
template<typename RadiusAt>
BendChange reshapeInside(Ring& ring, const BendLimit& bend, RadiusAt radiusAt)
{
    BendChange change;
    const int resolution = ring.radii.size();
    for (int bin = 0; bin < resolution; ++bin) {
        const double inside = QVector3D::dotProduct(binDirection(ring, bin), bend.inside);
        if (inside <= kInsideBinEpsilon) {
            continue;
        }
        const double radius = ring.radii.at(bin);
        const double reshaped = radiusAt(inside, radius);
        if (reshaped != radius) {
            change.changed = true;
            if (ring.measured.at(bin)) {
                change.largestMeasuredChange = std::max(change.largestMeasuredChange, std::abs(reshaped - radius));
            }
            ring.radii[bin] = reshaped;
            ring.measured[bin] = false;
        }
    }
    return change;
}

// Keeps every ring on the legs of a sharp bend in front of the corner's
// bisector plane and of its neighbors' planes by capping the bins that point
// into the bend. Where an inset ring was capped, the station ring's inner
// wall becomes the miter of the capped section, even where that moves a
// measured wall outward, so the inner wall meets the corner without a ridge.
// An elbow's rings are square to its arc, so its middle ring is only capped.
// Warns once per corner whose measured walls moved: by the caps, or on an
// elbow's middle ring by being read from its center.
void fitSharpBends(const QVector<Leg>& legs, const QVector<ChainSample>& samples, Chain& chain, QVector<Warning>& warnings)
{
    struct Corner {
        QVector<int> stationRings;
        BendChange change;
        bool isInsetCapped = false;
        bool isElbow = false;
    };
    QMap<int, Corner> corners;

    const auto capRing = [&](int ringIndex) {
        for (const BendLimit& bend : samples.at(ringIndex).bends) {
            const BendChange change = reshapeInside(chain.rings[ringIndex], bend, [&](double, double radius) {
                return std::min(radius, bend.maxInnerRadius);
            });
            Corner& corner = corners[bend.cornerLeg];
            corner.change.add(change);
            if (bend.role == BendLimit::Inset) {
                corner.isInsetCapped = corner.isInsetCapped || change.changed;
            }
            corner.isElbow = corner.isElbow || bend.role == BendLimit::Elbow;
        }
    };

    for (int i = 0; i < samples.size(); ++i) {
        if (const BendLimit* bend = samples.at(i).stationBend()) {
            Corner& corner = corners[bend->cornerLeg];
            corner.change.add(BendChange {false, insideWallShift(chain.rings.at(i), samples.at(i).stationOffset)});
            corner.stationRings.append(i);
        }
        capRing(i);
    }

    for (Corner& corner : corners) {
        if (corner.isInsetCapped && !corner.isElbow) {
            for (int i : std::as_const(corner.stationRings)) {
                const BendLimit& bend = *samples.at(i).stationBend();
                Ring& ring = chain.rings[i];
                const double cosine = halfTurnCosine(ring, bend);
                const double cappedSection = bend.maxInnerRadius * cosine;
                corner.change.add(reshapeInside(ring, bend, [&](double inside, double) {
                    return cappedSection / squareOverMiterRadius(cosine, inside);
                }));
                capRing(i);
            }
        }
    }

    for (auto it = corners.cbegin(); it != corners.cend(); ++it) {
        if (it.value().change.largestMeasuredChange > kClampWarningMeters) {
            const bool isChainStart = it.key() == kChainStartCorner;
            const ChainLeg& cornerLeg = chain.legs.at(isChainStart ? 0 : it.key());
            const QString& station = isChainStart ? chainStartStation(legs, cornerLeg) : chainEndStation(legs, cornerLeg);
            const QString message = it.value().isElbow
                                        ? QStringLiteral("The tube rounds the sharp bend at station %1, so its walls there move from the station's readings.")
                                        : QStringLiteral("Station %1's walls reach past the inside of its bend; the tube reshapes the inner wall to round the corner.");
            warnings.append(Warning {Warning::BendClamped, legs.at(cornerLeg.legIndex).tripId, station, message.arg(station)});
        }
    }
}

void buildRings(const QVector<Leg>& legs,
                const StationLegDirections& directions,
                Chain& chain,
                const Options& options,
                QVector<Warning>& warnings)
{
    const QVector<WalkedLeg> walked = walkedLegs(legs, chain, directions);
    const bool loop = isLoop(walked);
    const auto isGentleFan = [&](EndCap cap, const QVector3D& direction) {
        return !loop && cap == EndCap::Fan && inclinationWeight(direction, options) < kSteepWeight;
    };
    // A dome's ring sits past its station by the station's half-width, and a
    // through's past its junction by the arms' reach.
    const auto endOffset = [&](EndCap cap, const LrudRecord& record, double throughReach) {
        return cap == EndCap::Dome ? LegSection(record.lrud, options.defaultRadiusMeters).halfWidth : throughReach;
    };
    const bool isStartGentle = isGentleFan(chain.startCap, walked.first().direction());
    const bool isEndGentle = isGentleFan(chain.endCap, walked.last().direction());
    const bool hasStartReach = chain.startThroughReachMeters > 0.0;
    const bool hasEndReach = chain.endThroughReachMeters > 0.0;
    const ChainEnds ends {isStartGentle && !hasStartReach,
                          isEndGentle && !hasEndReach,
                          endOffset(chain.startCap, walked.first().startRing, chain.startThroughReachMeters),
                          endOffset(chain.endCap, walked.last().endRing, chain.endThroughReachMeters),
                          isStartGentle && hasStartReach,
                          isEndGentle && hasEndReach};
    const QVector<ChainSample> samples = chainSamples(walked, ends, options);
    const QVector<Frame> frames = chainFrames(samples, options);
    const int resolution = std::max(options.ringResolution, 1);

    QVector<RingFit> fits;
    fits.reserve(samples.size());
    QVector<int> fittedSamples;
    for (int i = 0; i < samples.size(); ++i) {
        fits.append(fitLrudRing(samples.at(i), frames.at(i), resolution));
        if (fits.last().filledBins >= kMinFilledBins) {
            fittedSamples.append(i);
        }
    }

    if (fittedSamples.isEmpty()) {
        warnings.append(chainWarning(legs, chain, Warning::NoPassageData,
                                     QStringLiteral("The passage from station %1 has no LRUD; the tube uses the default radius.")));
    }

    const auto tripOf = [&](int index) {
        return legs.at(chain.legs.at(samples.at(index).chainLeg).legIndex).tripId;
    };
    const auto nearestFitted = [&](int index) {
        const ChainSample& sample = samples.at(index);
        const auto trip = tripOf(index);
        QVector<int> sameTrip;
        std::copy_if(fittedSamples.cbegin(), fittedSamples.cend(), std::back_inserter(sameTrip), [&](int fitted) {
            return tripOf(fitted) == trip && std::abs(samples.at(fitted).chainLeg - sample.chainLeg) <= kSameTripBorrowLegs;
        });
        const QVector<int>& candidates = sameTrip.isEmpty() ? fittedSamples : sameTrip;
        return *std::min_element(candidates.cbegin(), candidates.cend(), [&](int a, int b) {
            return std::abs(samples.at(a).distanceAlongChain - sample.distanceAlongChain)
                   < std::abs(samples.at(b).distanceAlongChain - sample.distanceAlongChain);
        });
    };

    chain.rings.clear();
    chain.rings.reserve(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        Ring ring {samples.at(i).position, fits.at(i).radii, fits.at(i).binAngles, frames.at(i), fits.at(i).measured, samples.at(i).chainLeg};
        ring.isElbow = samples.at(i).isElbow;
        ring.section = fits.at(i).section;
        if (fits.at(i).filledBins < kMinFilledBins) {
            ring.measured = QVector<bool>(resolution, false);
            if (fittedSamples.isEmpty()) {
                ring.radii = QVector<double>(resolution, options.defaultRadiusMeters);
            } else {
                const RingFit borrowed = borrowedFit(fits.at(nearestFitted(i)), frames.at(i));
                ring.radii = borrowed.radii;
                ring.binAngles = borrowed.binAngles;
            }
            if (const BendLimit* bend = samples.at(i).stationBend()) {
                stretchAcrossMiter(ring, *bend);
            }
            reachPastStation(ring, samples.at(i).stationOffset);
        }
        chain.rings.append(ring);
    }

    blendElbowRings(samples, chain);
    fitSharpBends(legs, samples, chain, warnings);

    if (loop) {
        chain.startCapOffsetMeters = 0.0;
        chain.endCapOffsetMeters = 0.0;
    } else {
        chain.startCapOffsetMeters = capOffset(walked.first().startLrud, -walked.first().direction(), options);
        chain.endCapOffsetMeters = capOffset(walked.last().endLrud, walked.last().direction(), options);
    }
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

double cross2d(const QPointF& origin, const QPointF& a, const QPointF& b)
{
    return (a.x() - origin.x()) * (b.y() - origin.y()) - (a.y() - origin.y()) * (b.x() - origin.x());
}

// The longest edge squared over twice the area, as the mesh check measures
// slivers; infinite for a triangle wound clockwise or without area.
double aspectRatio(const QPointF& a, const QPointF& b, const QPointF& c)
{
    const double doubleArea = cross2d(a, b, c);
    if (doubleArea <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    const auto lengthSquared = [](const QPointF& v) { return QPointF::dotProduct(v, v); };
    return std::max({lengthSquared(b - a), lengthSquared(c - b), lengthSquared(a - c)}) / doubleArea;
}

bool containsPoint(const QVector<QPointF>& polygon, const QPointF& point)
{
    bool inside = false;
    for (int i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const QPointF& a = polygon.at(i);
        const QPointF& b = polygon.at(j);
        if ((a.y() > point.y()) != (b.y() > point.y())
            && point.x() < a.x() + (point.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y())) {
            inside = !inside;
        }
    }
    return inside;
}

// Whether the chord between polygon vertices i and j runs inside the
// polygon without crossing any of its edges.
bool isInteriorChord(const QVector<QPointF>& polygon, int i, int j)
{
    const int count = polygon.size();
    const QPointF& a = polygon.at(i);
    const QPointF& b = polygon.at(j);
    for (int k = 0; k < count; ++k) {
        const int next = (k + 1) % count;
        if (k == i || k == j || next == i || next == j) {
            continue;
        }
        const QPointF& c = polygon.at(k);
        const QPointF& d = polygon.at(next);
        const bool crosses = (cross2d(c, d, a) > 0.0) != (cross2d(c, d, b) > 0.0)
                             && (cross2d(a, b, c) > 0.0) != (cross2d(a, b, d) > 0.0);
        if (crosses) {
            return false;
        }
    }
    return containsPoint(polygon, (a + b) / 2.0);
}

using CapTriangle = std::array<int, 3>;

// Triangulates a simple polygon, counter-clockwise, so its worst triangle is
// as little of a sliver as it can be (dynamic programming over the chords),
// or returns nothing when the polygon has no such triangulation. A fan from
// one center leaves slivers toward the tips of a flat or thin section, where
// the bins lie close together.
QVector<CapTriangle> capTriangles(const QVector<QPointF>& polygon)
{
    struct Cost {
        double worst = std::numeric_limits<double>::infinity();
        double total = std::numeric_limits<double>::infinity();

        bool operator<(const Cost& other) const
        {
            return worst < other.worst || (worst == other.worst && total < other.total);
        }
    };

    const int count = polygon.size();
    if (count < 3) {
        return {};
    }
    const auto index = [count](int i, int j) { return i * count + j; };
    QVector<bool> isChord(count * count, false);
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            const bool isEdge = j == i + 1 || (i == 0 && j == count - 1);
            isChord[index(i, j)] = isEdge || isInteriorChord(polygon, i, j);
        }
    }

    QVector<Cost> cost(count * count);
    QVector<int> apex(count * count, -1);
    for (int i = 0; i + 1 < count; ++i) {
        cost[index(i, i + 1)] = Cost {0.0, 0.0};
    }
    for (int span = 2; span < count; ++span) {
        for (int i = 0; i + span < count; ++i) {
            const int j = i + span;
            if (!isChord.at(index(i, j))) {
                continue;
            }
            for (int k = i + 1; k < j; ++k) {
                if (!isChord.at(index(i, k)) || !isChord.at(index(k, j))) {
                    continue;
                }
                const double aspect = aspectRatio(polygon.at(i), polygon.at(k), polygon.at(j));
                const Cost& left = cost.at(index(i, k));
                const Cost& right = cost.at(index(k, j));
                const Cost candidate {std::max({aspect, left.worst, right.worst}), aspect + left.total + right.total};
                if (candidate < cost.at(index(i, j))) {
                    cost[index(i, j)] = candidate;
                    apex[index(i, j)] = k;
                }
            }
        }
    }
    if (!std::isfinite(cost.at(index(0, count - 1)).worst)) {
        return {};
    }

    QVector<CapTriangle> triangles;
    triangles.reserve(count - 2);
    QVector<std::pair<int, int>> pending {{0, count - 1}};
    while (!pending.isEmpty()) {
        const auto [i, j] = pending.takeLast();
        const int k = apex.at(index(i, j));
        if (k < 0) {
            continue;
        }
        triangles.append({i, k, j});
        pending.append({i, k});
        pending.append({k, j});
    }
    return triangles;
}

// The ring's vertices in its plane, counter-clockwise from frame.right
// toward frame.up.
QVector<QPointF> planarPolygon(const Ring& ring)
{
    QVector<QPointF> polygon;
    polygon.reserve(ring.vertices.size());
    for (const QVector3D& vertex : ring.vertices) {
        const QVector3D offset = vertex - ring.center;
        polygon.append(QPointF(QVector3D::dotProduct(offset, ring.frame.right), QVector3D::dotProduct(offset, ring.frame.up)));
    }
    return polygon;
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

    // The chain the following triangles belong to, and whether they are a
    // zip band.
    void setSource(int chain, bool isZipBand = false)
    {
        m_source = {chain, isZipBand};
    }

    // The following triangles, up to endCap, close the chain's tube at
    // station.
    void beginCap(const QUuid& caveId, const QString& station)
    {
        m_source.cap = m_debug.caps.size();
        m_debug.caps.append(cwPassageTubeBuilder::Cap {m_source.chain, caveId, station});
    }

    // The following triangles, up to endCap, close their trip's tube where
    // it runs on into neighborTrip's, and draw only while that trip is hidden.
    void beginTripBoundary(const QUuid& neighborTrip, const QUuid& caveId, const QString& station)
    {
        beginCap(caveId, station);
        m_neighborTrip = neighborTrip;
    }

    void endCap()
    {
        m_source.cap = -1;
        m_neighborTrip.reset();
    }

    // A cap's center, where the fan around it may fold sharply.
    quint32 addCapCenter(const QVector3D& position)
    {
        const quint32 index = addRing({position});
        m_debug.intendedCreaseVertices.insert(index);
        return index;
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

    // Side quads between two rings of resolution vertices, ring b further
    // along the tangent than ring a, apart from the quads starting at
    // skippedColumns. Each quad splits along its shorter diagonal, wound
    // counter-clockwise seen from outside.
    void addSides(const QUuid& tripId, quint32 a, quint32 b, int resolution, const QSet<int>& skippedColumns = {})
    {
        for (int k = 0; k < resolution; ++k) {
            if (skippedColumns.contains(k)) {
                continue;
            }
            const quint32 next = static_cast<quint32>((k + 1) % resolution);
            addQuad(tripId, a + k, a + next, b + k, b + next);
        }
    }

    // The strip from an arm's end ring, resolution vertices from endFirst, to
    // its mouth: per column, end ring vertex k fans to the mouth points up to
    // a bridge point and vertex k + 1 to the rest, the bridge chosen for the
    // shortest spokes, as a quad takes its shorter diagonal. mouth holds each
    // point's vertex; a triangle with two equal corners, where mouth points
    // are welded, is left out. atEnd winds the strip like side quads from the
    // end ring on to the mouth, for an arm arriving at its end.
    void addMouthStrip(const QUuid& tripId,
                       quint32 endFirst,
                       int resolution,
                       const QVector<quint32>& mouth,
                       const QVector<MouthPoint>& points,
                       bool atEnd)
    {
        const int count = mouth.size();
        QVector<int> columnFirst(resolution + 1, count);
        for (int i = count - 1; i >= 0; --i) {
            const int column = std::clamp(static_cast<int>(std::floor(points.at(i).column)), 0, resolution - 1);
            columnFirst[column] = i;
        }
        const QVector<QVector3D>& positions = m_mesh.positions;
        const auto addWound = [&](quint32 a, quint32 b, quint32 c) {
            if (atEnd) {
                addDistinctTriangle(tripId, a, b, c);
            } else {
                addDistinctTriangle(tripId, a, c, b);
            }
        };
        const auto mouthAt = [&](int i) { return mouth.at(i % count); };
        const auto spoke = [&](quint32 apex, int i) { return positions.at(apex).distanceToPoint(positions.at(mouthAt(i))); };
        for (int k = 0; k < resolution; ++k) {
            const quint32 low = endFirst + k;
            const quint32 high = endFirst + (k + 1) % resolution;
            const int first = columnFirst.at(k);
            const int last = columnFirst.at(k + 1);

            // Spokes from low to points first..bridge and from high to
            // bridge..last; the shortest total picks the bridge.
            double lowSpokes = 0.0;
            double highSpokes = 0.0;
            for (int i = first; i <= last; ++i) {
                highSpokes += spoke(high, i);
            }
            int bridge = first;
            double shortest = std::numeric_limits<double>::max();
            for (int i = first; i <= last; ++i) {
                lowSpokes += spoke(low, i);
                const double total = lowSpokes + highSpokes;
                if (total < shortest) {
                    shortest = total;
                    bridge = i;
                }
                highSpokes -= spoke(high, i);
            }

            for (int i = first; i < bridge; ++i) {
                addWound(low, mouthAt(i), mouthAt(i + 1));
            }
            addWound(low, mouthAt(bridge), high);
            for (int i = bridge; i < last; ++i) {
                addWound(high, mouthAt(i), mouthAt(i + 1));
            }
        }
    }

    // A fan closing a ring, given as its vertices in order. facingForward is
    // true when the fan faces along the ring's tangent (the end of a run),
    // false when it faces back.
    void addFan(const QUuid& tripId, quint32 center, const QVector<quint32>& ring, bool facingForward)
    {
        const int count = ring.size();
        for (int k = 0; k < count; ++k) {
            const quint32 current = ring.at(k);
            const quint32 next = ring.at((k + 1) % count);
            if (facingForward) {
                addTriangle(tripId, center, next, current);
            } else {
                addTriangle(tripId, center, current, next);
            }
        }
    }

    // Closes a planar ring, given as its vertices in order, with triangles
    // across it, or with a fan around fanCenter when the ring is not a simple
    // polygon. polygon is the ring in its plane, counter-clockwise from
    // frame.right toward frame.up; facingForward is true when the cap faces
    // along the ring's tangent.
    void addFlatCap(const QUuid& tripId, const QVector<quint32>& ring, const QVector<QPointF>& polygon, bool facingForward, const QVector3D& fanCenter)
    {
        const QVector<CapTriangle> triangles = capTriangles(polygon);
        if (triangles.isEmpty()) {
            addFan(tripId, addCapCenter(fanCenter), ring, facingForward);
            return;
        }
        for (const CapTriangle& triangle : triangles) {
            const quint32 a = ring.at(triangle.at(0));
            const quint32 b = ring.at(triangle.at(1));
            const quint32 c = ring.at(triangle.at(2));
            if (facingForward) {
                addTriangle(tripId, a, c, b);
            } else {
                addTriangle(tripId, a, b, c);
            }
        }
    }

    void finish()
    {
        const auto append = [&](const TripTriangles& trip) {
            const cwPassageTubeMesh::VertexRange range {static_cast<int>(m_mesh.indices.size()), static_cast<int>(trip.triangles.size())};
            m_mesh.indices.append(trip.triangles);
            for (const Source& source : trip.sources) {
                m_debug.triangleChain.append(source.chain);
                m_debug.triangleInZipBand.append(source.isZipBand);
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
    // Splits the quad along its shorter diagonal.
    void addQuad(const QUuid& tripId, quint32 a0, quint32 a1, quint32 b0, quint32 b1)
    {
        const QVector<QVector3D>& positions = m_mesh.positions;
        const float diagonalA0B1 = (positions.at(b1) - positions.at(a0)).lengthSquared();
        const float diagonalA1B0 = (positions.at(b0) - positions.at(a1)).lengthSquared();
        if (diagonalA0B1 <= diagonalA1B0) {
            addDistinctTriangle(tripId, a0, b0, b1);
            addDistinctTriangle(tripId, a0, b1, a1);
        } else {
            addDistinctTriangle(tripId, a0, b0, a1);
            addDistinctTriangle(tripId, b0, b1, a1);
        }
    }

    void addDistinctTriangle(const QUuid& tripId, quint32 a, quint32 b, quint32 c)
    {
        if (a != b && b != c && c != a) {
            addTriangle(tripId, a, b, c);
        }
    }

    struct Source {
        int chain = -1;
        bool isZipBand = false;
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

// Where a chain's rings and mouths landed in the mesh, so the zip bands can
// share their vertices; -1 where nothing was emitted.
struct ChainVertices {
    QVector<qint64> ringFirst;
    qint64 startMouthFirst = -1;
    qint64 endMouthFirst = -1;
};

QVector<quint32> consecutiveIndices(quint32 first, int count)
{
    QVector<quint32> indices(count);
    std::iota(indices.begin(), indices.end(), first);
    return indices;
}

QVector3D centroid(const QVector<QVector3D>& points)
{
    QVector3D sum;
    for (const QVector3D& point : points) {
        sum += point;
    }
    return sum / static_cast<float>(points.size());
}

ChainVertices addChainMesh(MeshWriter& writer, const QVector<Leg>& legs, const Chain& chain, QVector<Warning>& warnings)
{
    ChainVertices placed {QVector<qint64>(chain.rings.size(), -1)};
    const auto tripOfRing = [&](int ringIndex) {
        const ChainLeg& chainLeg = chain.legs.at(chain.rings.at(ringIndex).chainLeg);
        return legs.at(chainLeg.legIndex).tripId;
    };
    const QUuid& caveId = legs.at(chain.legs.first().legIndex).caveId;
    // The station a ring closes the tube at: the station ending its leg.
    const auto stationOfRing = [&](int ringIndex) {
        return chainEndStation(legs, chain.legs.at(chain.rings.at(ringIndex).chainLeg));
    };

    QHash<int, QSet<int>> removedColumnsByRing;
    for (const cwPassageTubeBuilder::JunctionWindow& window : chain.windows) {
        for (const QPair<int, int>& quad : window.removedQuads) {
            removedColumnsByRing[quad.first].insert(quad.second);
        }
    }

    // Runs of consecutive rings with valid frames; a torn chain has several.
    QVector<QVector<int>> runs(1);
    for (int i = 0; i < chain.rings.size(); ++i) {
        if (hasValidFrame(chain.rings.at(i))) {
            runs.last().append(i);
        } else if (!runs.last().isEmpty()) {
            runs.append(QVector<int>());
        }
    }
    const bool isTorn = runs.size() > 1 || runs.first().size() < chain.rings.size();
    if (isTorn) {
        warnings.append(chainWarning(legs, chain, Warning::ChainTorn,
                                     QStringLiteral("The passage from station %1 breaks where its cross-section could not be oriented.")));
    }

    const bool isClosedLoop = chain.startCap == EndCap::None && !isTorn;
    const int lastRing = chain.rings.size() - 1;

    for (const QVector<int>& run : std::as_const(runs)) {
        if (run.size() < 2) {
            continue;
        }
        const int resolution = chain.rings.at(run.first()).vertices.size();

        QVector<quint32> firstVertex;
        firstVertex.reserve(run.size());
        for (int i = 0; i < run.size(); ++i) {
            const bool closesLoop = isClosedLoop && i == run.size() - 1;
            firstVertex.append(closesLoop ? firstVertex.first() : writer.addRing(chain.rings.at(run.at(i)).vertices));
            placed.ringFirst[run.at(i)] = firstVertex.last();
        }

        // Segment s joins ring s - 1 to ring s and belongs to ring s's leg.
        for (int s = 1; s < run.size(); ++s) {
            writer.addSides(tripOfRing(run.at(s)), firstVertex.at(s - 1), firstVertex.at(s), resolution,
                            removedColumnsByRing.value(run.at(s - 1)));
        }

        // Where the trip changes, each trip has a disc across the shared ring
        // that closes its side while the other trip is hidden. A disc spans
        // only the columns whose side quads on its trip's side a junction
        // window kept.
        const auto addTripBoundary = [&](int position, const QUuid& before, const QUuid& after) {
            if (before == after) {
                return;
            }
            const int ringIndex = run.at(position);
            const Ring& ring = chain.rings.at(ringIndex);
            const QVector<QPointF> polygon = planarPolygon(ring);
            const auto addDisc = [&](const QUuid& trip, const QUuid& neighborTrip, int quadRing, bool facingForward) {
                const QSet<int> removed = removedColumnsByRing.value(quadRing);
                QVector<quint32> vertices;
                QVector<QPointF> keptPolygon;
                for (int k = 0; k < resolution; ++k) {
                    const int previousColumn = (k + resolution - 1) % resolution;
                    if (!removed.contains(previousColumn) || !removed.contains(k)) {
                        vertices.append(firstVertex.at(position) + k);
                        keptPolygon.append(polygon.at(k));
                    }
                }
                if (vertices.size() < kMinDiscVertices) {
                    return;
                }
                writer.beginTripBoundary(neighborTrip, caveId, stationOfRing(ringIndex));
                writer.addFlatCap(trip, vertices, keptPolygon, facingForward, ring.center);
                writer.endCap();
            };
            const int quadRingBefore = position > 0 ? run.at(position - 1) : run.at(run.size() - 2);
            addDisc(before, after, quadRingBefore, true);
            addDisc(after, before, ringIndex, false);
        };
        for (int s = 1; s + 1 < run.size(); ++s) {
            addTripBoundary(s, tripOfRing(run.at(s)), tripOfRing(run.at(s + 1)));
        }
        if (isClosedLoop) {
            addTripBoundary(0, tripOfRing(run.last()), tripOfRing(run.at(1)));
            continue;
        }

        // A mouth adds one strip past the end ring; a zipped mouth's strip is
        // added with its zip band, which knows the mouth vertices welded to
        // the through, and one left unzipped is closed by a fan. A dead end
        // closes flat across its end ring; a steep one runs one strip on to
        // its floor or ceiling first.
        const auto addCap = [&](int position, EndCap cap, double pitOffset, bool atEnd, const QVector<MouthPoint>& mouth,
                                const std::optional<QVector3D>& mouthFanCenter, const QString& station) -> qint64 {
            const Ring& ring = chain.rings.at(run.at(position));
            const QUuid tripId = tripOfRing(run.at(atEnd ? position : position + 1));
            const QVector3D outward = atEnd ? ring.frame.tangent : -ring.frame.tangent;
            quint32 capRing = firstVertex.at(position);
            const auto addStrip = [&](const QVector<QVector3D>& end) {
                const quint32 endFirst = writer.addRing(end);
                if (atEnd) {
                    writer.addSides(tripId, capRing, endFirst, resolution);
                } else {
                    writer.addSides(tripId, endFirst, capRing, resolution);
                }
                capRing = endFirst;
            };

            if (!mouth.isEmpty()) {
                QVector<QVector3D> positions;
                positions.reserve(mouth.size());
                for (const MouthPoint& point : mouth) {
                    positions.append(point.position);
                }
                const quint32 mouthFirst = writer.addRing(positions);
                if (cap == EndCap::Mouth) {
                    return mouthFirst;
                }
                const QVector<quint32> mouthVertices = consecutiveIndices(mouthFirst, mouth.size());
                writer.addMouthStrip(tripId, capRing, resolution, mouthVertices, mouth, atEnd);
                writer.beginCap(caveId, station);
                writer.addFan(tripId, writer.addCapCenter(mouthFanCenter.value_or(centroid(positions))), mouthVertices, atEnd);
                writer.endCap();
                return -1;
            }

            if (cap == EndCap::Dome) {
                const double radiusSum = std::accumulate(ring.vertices.cbegin(), ring.vertices.cend(), 0.0,
                                                         [&](double sum, const QVector3D& vertex) {
                                                             return sum + vertex.distanceToPoint(ring.center);
                                                         });
                const double depth = radiusSum / ring.vertices.size();
                QVector<QVector3D> halfRing;
                halfRing.reserve(resolution);
                for (const QVector3D& vertex : ring.vertices) {
                    halfRing.append(ring.center + (vertex - ring.center) * kDomeHalfRingScale
                                    + outward * static_cast<float>(depth * kDomeHalfRingDepth));
                }
                addStrip(halfRing);
                writer.addFan(tripId, writer.addCapCenter(ring.center + outward * static_cast<float>(depth)),
                              consecutiveIndices(capRing, resolution), atEnd);
                return -1;
            }

            QVector3D fanCenter = ring.center + (centroid(ring.vertices) - ring.center) * kFanCenterBlend;
            const double offset = ring.frame.inclinationWeight > kSteepWeight ? pitOffset : 0.0;
            if (offset > 0.0) {
                QVector<QVector3D> pitEnd;
                pitEnd.reserve(resolution);
                for (const QVector3D& vertex : ring.vertices) {
                    pitEnd.append(vertex + outward * static_cast<float>(offset));
                }
                addStrip(pitEnd);
                fanCenter = centroid(pitEnd);
            }
            writer.beginCap(caveId, station);
            writer.addFlatCap(tripId, consecutiveIndices(capRing, resolution), planarPolygon(ring), atEnd, fanCenter);
            writer.endCap();
            return -1;
        };
        const bool isChainStart = run.first() == 0;
        const bool isChainEnd = run.last() == lastRing;
        const qint64 startMouth = addCap(0, isChainStart ? chain.startCap : EndCap::Fan,
                                         isChainStart ? chain.startCapOffsetMeters : 0.0, false,
                                         isChainStart ? chain.startMouth : QVector<MouthPoint>(),
                                         chain.startFanCenter,
                                         isChainStart ? chainStartStation(legs, chain.legs.first()) : stationOfRing(run.first()));
        const qint64 endMouth = addCap(run.size() - 1, isChainEnd ? chain.endCap : EndCap::Fan,
                                       isChainEnd ? chain.endCapOffsetMeters : 0.0, true,
                                       isChainEnd ? chain.endMouth : QVector<MouthPoint>(),
                                       chain.endFanCenter, stationOfRing(run.last()));
        if (isChainStart) {
            placed.startMouthFirst = startMouth;
        }
        if (isChainEnd) {
            placed.endMouthFirst = endMouth;
        }
    }
    return placed;
}

// Each window's zip band and its arms' strips to their mouths, sharing the
// through's ring vertices and the arms' mouth vertices.
void addZipBands(MeshWriter& writer, const QVector<Chain>& chains, const QVector<ChainVertices>& placed)
{
    using ZipCorner = cwPassageTubeBuilder::ZipCorner;
    using WindowMouth = cwPassageTubeBuilder::WindowMouth;
    for (int c = 0; c < chains.size(); ++c) {
        for (const cwPassageTubeBuilder::JunctionWindow& window : chains.at(c).windows) {
            const auto mouthFirstOf = [&](const WindowMouth& mouth) {
                const ChainVertices& arm = placed.at(mouth.armChain);
                return mouth.isArmStart ? arm.startMouthFirst : arm.endMouthFirst;
            };
            const auto indexOf = [&](const ZipCorner& corner) {
                const qint64 first = corner.ring == ZipCorner::kMouthRing ? mouthFirstOf(window.mouths.at(corner.mouth))
                                                                          : placed.at(c).ringFirst.at(corner.ring);
                return first < 0 ? first : first + corner.column;
            };

            for (const WindowMouth& windowMouth : window.mouths) {
                const ChainVertices& arm = placed.at(windowMouth.armChain);
                const qint64 endRingFirst = windowMouth.isArmStart ? arm.ringFirst.first() : arm.ringFirst.last();
                if (mouthFirstOf(windowMouth) < 0 || endRingFirst < 0) {
                    continue;
                }
                QVector<quint32> mouth;
                mouth.reserve(windowMouth.mouthCorners.size());
                for (const ZipCorner& corner : windowMouth.mouthCorners) {
                    const qint64 index = indexOf(corner);
                    if (index >= 0) {
                        mouth.append(static_cast<quint32>(index));
                    }
                }
                if (mouth.size() == windowMouth.mouthCorners.size()) {
                    const Chain& armChain = chains.at(windowMouth.armChain);
                    const int resolution = (windowMouth.isArmStart ? armChain.rings.first() : armChain.rings.last()).vertices.size();
                    writer.setSource(windowMouth.armChain);
                    writer.addMouthStrip(windowMouth.tripId, static_cast<quint32>(endRingFirst), resolution, mouth,
                                         windowMouth.isArmStart ? armChain.startMouth : armChain.endMouth, !windowMouth.isArmStart);
                }
            }

            writer.setSource(c, true);
            for (const std::array<ZipCorner, 3>& triangle : window.zip) {
                const qint64 a = indexOf(triangle.at(0));
                const qint64 b = indexOf(triangle.at(1));
                const qint64 d = indexOf(triangle.at(2));
                if (a >= 0 && b >= 0 && d >= 0) {
                    writer.addTriangle(window.tripId, static_cast<quint32>(a), static_cast<quint32>(b), static_cast<quint32>(d));
                }
            }
        }
    }
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

    Debug debug;
    debug.legs = extractLegs(region, positionsByCave, debug.warnings);
    replaceShaftReadings(debug.legs, debug.warnings);
    debug.chains = buildChains(debug.legs, options.defaultRadiusMeters);
    classifyChainEnds(debug.legs, debug.chains);

    const StationLegDirections directions = stationLegDirections(debug.legs);
    for (Chain& chain : debug.chains) {
        buildRings(debug.legs, directions, chain, options, debug.warnings);
    }
    cwPassageTubeJunction::stitchJunctions(debug.legs, debug.chains, options, debug.warnings);

    MeshWriter writer(debug);
    QVector<ChainVertices> placed;
    placed.reserve(debug.chains.size());
    for (int c = 0; c < debug.chains.size(); ++c) {
        Chain& chain = debug.chains[c];
        placeChainVertices(chain);
        writer.setSource(c);
        placed.append(addChainMesh(writer, debug.legs, chain, debug.warnings));
    }
    addZipBands(writer, debug.chains, placed);
    writer.finish();
    debug.mesh.warnings = debug.warnings;
    return debug;
}
