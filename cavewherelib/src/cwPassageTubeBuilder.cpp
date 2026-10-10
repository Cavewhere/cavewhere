/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwUnits.h"

//Qt includes
#include <QSet>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr int kMinFilledBins = 4;
constexpr int kTwistDistributionSamples = 4;
constexpr double kMinSampleWeight = 0.05;
constexpr double kSteepWeight = 0.5;
constexpr double kAlongLegDegrees = 45.0;
constexpr double kShaftRatio = 3.0;
constexpr double kMinSampleSpacingMeters = 0.01;
constexpr double kMinRingSpacingMeters = 0.05;
constexpr double kCornerInsetMeters = 0.5;
constexpr double kCornerInsetLegFraction = 0.25;
constexpr int kDomeRings = 3;
constexpr int kMinLoopRings = 3;
constexpr float kMinAxisLengthSquared = 0.25f;
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
// positioned neighbors in the chunk. A station on a vertical chord takes the
// facing of the nearest station that has one; a chunk without any faces north.
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

    QVector<std::optional<QVector3D>> chordFacings(chunk.stations.size());
    for (int p = 0; p < positioned.size(); ++p) {
        const int previous = positioned.at(std::max(p - 1, 0));
        const int next = positioned.at(std::min<int>(p + 1, positioned.size() - 1));
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

QVector<Leg> extractLegs(const cwCavingRegionData& region,
                         const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                         QVector<Warning>& warnings)
{
    QVector<Leg> legs;
    for (const cwCaveData& cave : region.caves) {
        const auto positionsIt = positionsByCave.constFind(cave.id);
        if (positionsIt == positionsByCave.constEnd()) {
            continue;
        }
        const cwStationPositionLookup& positions = positionsIt.value();

        for (const cwTripData& trip : cave.trips) {
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
    return legs;
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

using Frame = cwPassageTubeBuilder::Frame;
using Ring = cwPassageTubeBuilder::Ring;
using Options = cwPassageTubeBuilder::Options;

// A station occurrence's LRUD with the facing they were read along.
struct LrudRecord {
    cwStationLrud lrud;
    QVector3D facing;
};

// One place along a chain where a ring is fitted. chainLeg indexes
// Chain::legs: the leg the sample lies on or ends.
struct ChainSample {
    QVector3D position;
    QVector3D tangent;
    double distanceAlongChain = 0.0;
    QVector<LrudRecord> records;
    int chainLeg = 0;
    bool isStation = false;
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

// Every passage LRUD record of each station, from every leg that touches it,
// keyed by cave and canonical station name.
using StationRecords = QHash<StationKey, QVector<LrudRecord>>;

StationKey canonicalStation(const QUuid& caveId, const QString& station)
{
    return {caveId, cwStation::canonicalKey(station)};
}

StationRecords stationRecords(const QVector<Leg>& legs)
{
    StationRecords records;
    for (const Leg& leg : legs) {
        records[canonicalStation(leg.caveId, leg.from)].append({leg.fromPassageLrud, leg.fromFacing});
        records[canonicalStation(leg.caveId, leg.to)].append({leg.toPassageLrud, leg.toFacing});
    }
    return records;
}

// How far the station's walls reach along outward: the largest projection of
// any wall offset of its records, or nullopt when it has no readings.
std::optional<double> farWallDistance(const QVector<LrudRecord>& records, const QVector3D& outward)
{
    std::optional<double> farthest;
    const auto reach = [&](const std::optional<double>& reading, const QVector3D& direction) {
        if (reading) {
            const double distance = *reading * QVector3D::dotProduct(direction, outward);
            farthest = std::max(farthest.value_or(distance), distance);
        }
    };
    for (const LrudRecord& record : records) {
        const QVector3D right = rightOfFacing(record.facing);
        reach(record.lrud.left, -right);
        reach(record.lrud.right, right);
        reach(record.lrud.up, kWorldUp);
        reach(record.lrud.down, -kWorldUp);
    }
    return farthest;
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

QVector<ChainSample> chainSamples(const QVector<WalkedLeg>& walked, const Options& options)
{
    const bool loop = isLoop(walked);
    const double spacing = std::max(options.sampleSpacingMeters, kMinSampleSpacingMeters);
    const float minBendCosine = static_cast<float>(std::cos(qDegreesToRadians(options.subdivideBendDegrees)));

    const auto isSharpBendAfter = [&](int legIndex) {
        const bool hasNext = loop || legIndex + 1 < walked.size();
        if (!hasNext) {
            return false;
        }
        const WalkedLeg& next = walked.at((legIndex + 1) % walked.size());
        return QVector3D::dotProduct(walked.at(legIndex).direction(), next.direction()) < minBendCosine;
    };

    QVector<ChainSample> samples;
    double distance = 0.0;
    for (int legIndex = 0; legIndex < walked.size(); ++legIndex) {
        const WalkedLeg& leg = walked.at(legIndex);
        const QVector3D direction = leg.direction();

        if (legIndex == 0) {
            const QVector3D tangent = loop ? bisector(walked.last().direction(), direction) : direction;
            appendSample(samples, ChainSample {leg.start, tangent, 0.0, {leg.startRing}, legIndex, true});
        }

        const double length = leg.start.distanceToPoint(leg.end);
        const int intervals = std::max(1, static_cast<int>(std::lround(length / spacing)));
        QVector<double> fractions;
        for (int i = 1; i < intervals; ++i) {
            fractions.append(static_cast<double>(i) / intervals);
        }

        const double cornerInsetFraction = std::min(kCornerInsetMeters, kCornerInsetLegFraction * length) / length;
        const bool startsAtSharpBend = legIndex > 0 ? isSharpBendAfter(legIndex - 1)
                                                    : loop && isSharpBendAfter(walked.size() - 1);
        if (startsAtSharpBend) {
            fractions.append(cornerInsetFraction);
        }
        if (isSharpBendAfter(legIndex)) {
            fractions.append(1.0 - cornerInsetFraction);
        }
        std::sort(fractions.begin(), fractions.end());

        for (double t : std::as_const(fractions)) {
            appendSample(samples, ChainSample {
                leg.start + (leg.end - leg.start) * static_cast<float>(t),
                direction,
                distance + length * t,
                {lerpRecord(leg.startRing, leg.endRing, t)},
                legIndex,
                false
            });
        }

        distance += length;
        const bool isLast = legIndex == walked.size() - 1;
        QVector3D endTangent = direction;
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
        appendSample(samples, ChainSample {leg.end, endTangent, distance, endRecords, legIndex, true});
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

QVector3D rotateAbout(const QVector3D& axis, const QVector3D& v, double angle)
{
    const float c = static_cast<float>(std::cos(angle));
    const float s = static_cast<float>(std::sin(angle));
    return v * c + QVector3D::crossProduct(axis, v) * s + axis * QVector3D::dotProduct(axis, v) * (1.0f - c);
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
    return frames;
}

// Distance from the center to the ellipse with semi-axes a and b along the
// direction (cosTheta, sinTheta).
double ellipsePolarRadius(double a, double b, double cosTheta, double sinTheta)
{
    return a * b / std::hypot(b * cosTheta, a * sinTheta);
}

// The record's walls as seen across the ring: each side wall foreshortened to
// its component along frame.right, swapped when the record faces the other
// way. A record facing along the ring's tangent keeps only up and down.
cwStationLrud lrudAcross(const LrudRecord& record, const Frame& frame)
{
    cwStationLrud across {std::nullopt, std::nullopt, record.lrud.up, record.lrud.down};
    const float component = QVector3D::dotProduct(rightOfFacing(record.facing), frame.right);
    const double scale = std::abs(component);
    if (scale < kMinProjectedLength) {
        return across;
    }
    const auto projected = [scale](const std::optional<double>& reading) -> std::optional<double> {
        if (reading) {
            return std::max(cwPassageTubeBuilder::kMinRadiusMeters, *reading * scale);
        }
        return std::nullopt;
    };
    const bool facesSameWay = component >= 0.0f;
    across.right = projected(facesSameWay ? record.lrud.right : record.lrud.left);
    across.left = projected(facesSameWay ? record.lrud.left : record.lrud.right);
    return across;
}

struct RingFit {
    QVector<double> radii;
    QVector<bool> measured;
    int filledBins = 0;
};

// Samples the LRUD quadrant ellipse into the ring's bins and fills the empty
// bins between measured ones. Bins are left at zero when fewer than
// kMinFilledBins are measured.
RingFit fitLrudRing(const ChainSample& sample, const Frame& frame, int resolution)
{
    RingFit fit {QVector<double>(resolution, 0.0), QVector<bool>(resolution, false), 0};
    const bool hasLrudWeight = 1.0 - frame.inclinationWeight >= kMinSampleWeight;
    if (hasLrudWeight) {
        QVector<cwStationLrud> across;
        across.reserve(sample.records.size());
        for (const LrudRecord& record : sample.records) {
            across.append(lrudAcross(record, frame));
        }
        for (int bin = 0; bin < resolution; ++bin) {
            const double theta = kTwoPi * bin / resolution;
            const double cosTheta = std::cos(theta);
            const double sinTheta = std::sin(theta);
            const bool usesHorizontal = std::abs(cosTheta) > kAxisEpsilon;
            const bool usesVertical = std::abs(sinTheta) > kAxisEpsilon;
            for (const cwStationLrud& lrud : std::as_const(across)) {
                const std::optional<double> a = cosTheta >= 0.0 ? lrud.right : lrud.left;
                const std::optional<double> b = sinTheta >= 0.0 ? lrud.up : lrud.down;
                if ((usesHorizontal && !a) || (usesVertical && !b)) {
                    continue;
                }
                double radius = usesHorizontal ? *a : *b;
                if (usesHorizontal && usesVertical) {
                    radius = ellipsePolarRadius(*a, *b, cosTheta, sinTheta);
                }
                fit.radii[bin] = fit.measured.at(bin) ? std::max(fit.radii.at(bin), radius) : radius;
                fit.measured[bin] = true;
            }
        }
    }

    fit.filledBins = static_cast<int>(std::count(fit.measured.cbegin(), fit.measured.cend(), true));
    if (fit.filledBins < kMinFilledBins) {
        return fit;
    }

    for (int bin = 0; bin < resolution; ++bin) {
        if (fit.measured.at(bin)) {
            continue;
        }
        const auto wrapped = [resolution](int index) { return (index + resolution) % resolution; };
        int before = 1;
        while (!fit.measured.at(wrapped(bin - before))) {
            ++before;
        }
        int after = 1;
        while (!fit.measured.at(wrapped(bin + after))) {
            ++after;
        }
        const double radiusBefore = fit.radii.at(wrapped(bin - before));
        const double radiusAfter = fit.radii.at(wrapped(bin + after));
        fit.radii[bin] = radiusBefore + (radiusAfter - radiusBefore) * before / (before + after);
    }
    return fit;
}

// A ring without data takes the nearest fitted ring. In a pit that ring's
// ceiling and floor would land on horizontal axes through the rolled frame,
// so the pit is round at the fitted ring's horizontal half-width instead.
QVector<double> borrowedRadii(const RingFit& fitted, const Frame& frame)
{
    if (frame.inclinationWeight <= kSteepWeight) {
        return fitted.radii;
    }
    const int resolution = fitted.radii.size();
    const int rightBin = 0;
    const int leftBin = resolution / 2;
    const double halfWidth = (fitted.radii.at(rightBin) + fitted.radii.at(leftBin)) / 2.0;
    return QVector<double>(resolution, halfWidth);
}

// Level dead ends sit flush with the end station; pits extend by up or down.
double capOffset(const cwStationLrud& lrud, const QVector3D& outward, const Frame& endFrame)
{
    const std::optional<double> offset = outward.z() >= 0.0f ? lrud.up : lrud.down;
    return endFrame.inclinationWeight * offset.value_or(0.0);
}

QString chainStartStation(const QVector<Leg>& legs, const ChainLeg& chainLeg)
{
    const Leg& leg = legs.at(chainLeg.legIndex);
    return chainLeg.reversed ? leg.to : leg.from;
}

QString chainEndStation(const QVector<Leg>& legs, const ChainLeg& chainLeg)
{
    const Leg& leg = legs.at(chainLeg.legIndex);
    return chainLeg.reversed ? leg.from : leg.to;
}

// A warning about the chain, reported at its first station. message takes
// the station name as %1.
Warning chainWarning(const QVector<Leg>& legs, const Chain& chain, Warning::Type type, const QString& message)
{
    const ChainLeg& firstLeg = chain.legs.first();
    const QString station = chainStartStation(legs, firstLeg);
    return Warning {type, legs.at(firstLeg.legIndex).tripId, station, message.arg(station)};
}

double meanRadius(const Ring& ring)
{
    if (ring.radii.isEmpty()) {
        return 0.0;
    }
    return std::accumulate(ring.radii.cbegin(), ring.radii.cend(), 0.0) / ring.radii.size();
}

using EndCap = cwPassageTubeBuilder::EndCap;

// A dome reaches the junction's far wall along the arm, or as far as the arm's
// own end ring when the junction has no readings.
double domeHeight(const StationRecords& records, const StationKey& junction, const QVector3D& outward, const Ring& armEnd)
{
    const std::optional<double> farWall = farWallDistance(records.value(junction), outward);
    if (farWall) {
        return std::max(cwPassageTubeBuilder::kMinRadiusMeters, *farWall);
    }
    return meanRadius(armEnd);
}

void buildRings(const QVector<Leg>& legs,
                const StationLegDirections& directions,
                const StationRecords& records,
                Chain& chain,
                const Options& options,
                QVector<Warning>& warnings)
{
    const QVector<WalkedLeg> walked = walkedLegs(legs, chain, directions);
    const QVector<ChainSample> samples = chainSamples(walked, options);
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

    const auto nearestFitted = [&](int index) {
        const double distance = samples.at(index).distanceAlongChain;
        return *std::min_element(fittedSamples.cbegin(), fittedSamples.cend(), [&](int a, int b) {
            return std::abs(samples.at(a).distanceAlongChain - distance)
                   < std::abs(samples.at(b).distanceAlongChain - distance);
        });
    };

    chain.rings.clear();
    chain.rings.reserve(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        Ring ring {samples.at(i).position, fits.at(i).radii, frames.at(i), fits.at(i).measured, samples.at(i).chainLeg};
        if (fits.at(i).filledBins < kMinFilledBins) {
            ring.measured = QVector<bool>(resolution, false);
            ring.radii = fittedSamples.isEmpty()
                             ? QVector<double>(resolution, options.defaultRadiusMeters)
                             : borrowedRadii(fits.at(nearestFitted(i)), frames.at(i));
        }
        chain.rings.append(ring);
    }

    if (isLoop(walked)) {
        chain.startCapOffsetMeters = 0.0;
        chain.endCapOffsetMeters = 0.0;
    } else {
        chain.startCapOffsetMeters = capOffset(walked.first().startLrud, -walked.first().direction(), frames.first());
        chain.endCapOffsetMeters = capOffset(walked.last().endLrud, walked.last().direction(), frames.last());
    }

    const auto domeHeightAt = [&](EndCap cap, const ChainLeg& chainLeg, const QString& junction,
                                  const QVector3D& outward, const Ring& armEnd) {
        if (cap == EndCap::Dome) {
            return domeHeight(records, canonicalStation(legs.at(chainLeg.legIndex).caveId, junction), outward, armEnd);
        }
        return 0.0;
    };
    const ChainLeg& firstLeg = chain.legs.first();
    const ChainLeg& lastLeg = chain.legs.last();
    chain.startDomeHeightMeters = domeHeightAt(chain.startCap, firstLeg, chainStartStation(legs, firstLeg),
                                               -walked.first().direction(), chain.rings.first());
    chain.endDomeHeightMeters = domeHeightAt(chain.endCap, lastLeg, chainEndStation(legs, lastLeg),
                                             walked.last().direction(), chain.rings.last());
}

// A chain end at a junction that another chain passes through is an arm and
// gets a dome; every other open end gets a fan, and loops get no caps.
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
    }

    const auto endCap = [&](const StationKey& station) {
        const bool isArm = degree.value(station) > 2 && passedThrough.contains(station);
        return isArm ? EndCap::Dome : EndCap::Fan;
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
            chain.startCap = endCap(start);
            chain.endCap = endCap(end);
        }
    }
}

bool isFinite(const QVector3D& v)
{
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

// A ring whose frame could not be built tears the chain.
bool hasValidFrame(const Ring& ring)
{
    return isFinite(ring.center)
           && isFinite(ring.frame.tangent)
           && isFinite(ring.frame.up)
           && isFinite(ring.frame.right)
           && ring.frame.right.lengthSquared() > kMinAxisLengthSquared;
}

// Places the ring's vertices. A vertex that would land behind the previous
// ring's plane, folding the tube inside out on the inside of a bend, slides
// along its own ray onto that plane.
void placeRingVertices(Ring& ring, const Ring* previous)
{
    const int resolution = ring.radii.size();
    ring.vertices.resize(resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        const double theta = kTwoPi * bin / resolution;
        const QVector3D direction = ring.frame.right * static_cast<float>(std::cos(theta))
                                    + ring.frame.up * static_cast<float>(std::sin(theta));
        double radius = ring.radii.at(bin);
        if (previous != nullptr) {
            const QVector3D& planeNormal = previous->frame.tangent;
            const QVector3D vertex = ring.center + direction * static_cast<float>(radius);
            if (QVector3D::dotProduct(vertex - previous->center, planeNormal) < 0.0f) {
                const float approach = QVector3D::dotProduct(direction, planeNormal);
                const double toPlane = approach < 0.0f
                                           ? QVector3D::dotProduct(previous->center - ring.center, planeNormal) / approach
                                           : 0.0;
                radius = std::max(toPlane, cwPassageTubeBuilder::kMinRadiusMeters);
            }
        }
        ring.vertices[bin] = ring.center + direction * static_cast<float>(radius);
    }
}

// A loop's last ring repeats its first, so the first ring folds against the
// ring before that one.
void placeChainVertices(Chain& chain)
{
    const bool isLoop = chain.startCap == EndCap::None && chain.rings.size() >= kMinLoopRings;
    const Ring* previous = nullptr;
    if (isLoop) {
        const Ring& closing = chain.rings.at(chain.rings.size() - 2);
        if (hasValidFrame(closing)) {
            previous = &closing;
        }
    }
    for (Ring& ring : chain.rings) {
        if (hasValidFrame(ring)) {
            placeRingVertices(ring, previous);
            previous = &ring;
        } else {
            ring.vertices.clear();
        }
    }
}

// Collects triangles per trip so each trip's indices end up contiguous.
class MeshWriter
{
public:
    explicit MeshWriter(cwPassageTubeMesh& mesh) : m_mesh(mesh) {}

    quint32 addVertex(const QVector3D& position)
    {
        return addRing({position});
    }

    quint32 addRing(const QVector<QVector3D>& vertices)
    {
        const quint32 first = static_cast<quint32>(m_mesh.positions.size());
        m_mesh.positions.append(vertices);
        return first;
    }

    void addTriangle(const QUuid& tripId, quint32 a, quint32 b, quint32 c)
    {
        auto it = m_trianglesByTrip.find(tripId);
        if (it == m_trianglesByTrip.end()) {
            m_tripOrder.append(tripId);
            it = m_trianglesByTrip.insert(tripId, {});
        }
        it->append(QVector<quint32> {a, b, c});
    }

    // Side quads between two rings of resolution vertices, ring b further
    // along the tangent than ring a. Each quad splits along its shorter
    // diagonal, wound counter-clockwise seen from outside.
    void addSides(const QUuid& tripId, quint32 a, quint32 b, int resolution)
    {
        const QVector<QVector3D>& positions = m_mesh.positions;
        for (int k = 0; k < resolution; ++k) {
            const quint32 next = static_cast<quint32>((k + 1) % resolution);
            const quint32 a0 = a + k;
            const quint32 a1 = a + next;
            const quint32 b1 = b + next;
            const quint32 b0 = b + k;
            const float diagonalA0B1 = (positions.at(b1) - positions.at(a0)).lengthSquared();
            const float diagonalA1B0 = (positions.at(b0) - positions.at(a1)).lengthSquared();
            if (diagonalA0B1 <= diagonalA1B0) {
                addTriangle(tripId, a0, b0, b1);
                addTriangle(tripId, a0, b1, a1);
            } else {
                addTriangle(tripId, a0, b0, a1);
                addTriangle(tripId, b0, b1, a1);
            }
        }
    }

    // A fan closing a ring. facingForward is true when the fan faces along
    // the ring's tangent (the end of a run), false when it faces back.
    void addFan(const QUuid& tripId, quint32 center, quint32 ring, int resolution, bool facingForward)
    {
        for (int k = 0; k < resolution; ++k) {
            const quint32 current = ring + k;
            const quint32 next = ring + (k + 1) % resolution;
            if (facingForward) {
                addTriangle(tripId, center, next, current);
            } else {
                addTriangle(tripId, center, current, next);
            }
        }
    }

    void finish()
    {
        for (const QUuid& tripId : std::as_const(m_tripOrder)) {
            const QVector<quint32>& triangles = m_trianglesByTrip.value(tripId);
            m_mesh.tripIndexRanges.append({static_cast<int>(m_mesh.indices.size()), static_cast<int>(triangles.size())});
            m_mesh.tripUuids.append(tripId);
            m_mesh.indices.append(triangles);
        }
    }

private:
    cwPassageTubeMesh& m_mesh;
    QVector<QUuid> m_tripOrder;
    QHash<QUuid, QVector<quint32>> m_trianglesByTrip;
};

// A Loch-style dome past the ring: kDomeRings rings shrinking on a quarter
// ellipse out to an apex height past the ring.
void addDome(MeshWriter& writer, const QUuid& tripId, const Ring& ring, quint32 ringFirst, bool atEnd, double height)
{
    const int resolution = ring.vertices.size();
    const QVector3D outward = atEnd ? ring.frame.tangent : -ring.frame.tangent;

    quint32 previous = ringFirst;
    for (int i = 1; i <= kDomeRings; ++i) {
        const double angle = (std::numbers::pi / 2.0) * i / (kDomeRings + 1);
        const QVector3D center = ring.center + outward * static_cast<float>(height * std::sin(angle));
        const float scale = static_cast<float>(std::cos(angle));
        QVector<QVector3D> vertices;
        vertices.reserve(resolution);
        for (const QVector3D& vertex : ring.vertices) {
            vertices.append(center + (vertex - ring.center) * scale);
        }
        const quint32 current = writer.addRing(vertices);
        if (atEnd) {
            writer.addSides(tripId, previous, current, resolution);
        } else {
            writer.addSides(tripId, current, previous, resolution);
        }
        previous = current;
    }
    const quint32 apex = writer.addVertex(ring.center + outward * static_cast<float>(height));
    writer.addFan(tripId, apex, previous, resolution, atEnd);
}

void addChainMesh(MeshWriter& writer, const QVector<Leg>& legs, const Chain& chain, QVector<Warning>& warnings)
{
    const auto tripOfRing = [&](int ringIndex) {
        const ChainLeg& chainLeg = chain.legs.at(chain.rings.at(ringIndex).chainLeg);
        return legs.at(chainLeg.legIndex).tripId;
    };

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
        }

        // Segment s joins ring s - 1 to ring s and belongs to ring s's leg.
        for (int s = 1; s < run.size(); ++s) {
            writer.addSides(tripOfRing(run.at(s)), firstVertex.at(s - 1), firstVertex.at(s), resolution);
        }

        // Where the trip changes, each trip closes its own side of the
        // shared ring so hiding either one leaves the other capped.
        const auto addTripBoundary = [&](int position, const QUuid& before, const QUuid& after) {
            if (before == after) {
                return;
            }
            const quint32 center = writer.addVertex(chain.rings.at(run.at(position)).center);
            writer.addFan(before, center, firstVertex.at(position), resolution, true);
            writer.addFan(after, center, firstVertex.at(position), resolution, false);
        };
        for (int s = 1; s + 1 < run.size(); ++s) {
            addTripBoundary(s, tripOfRing(run.at(s)), tripOfRing(run.at(s + 1)));
        }
        if (isClosedLoop) {
            addTripBoundary(0, tripOfRing(run.last()), tripOfRing(run.at(1)));
            continue;
        }

        const auto addCap = [&](int position, EndCap cap, double pitOffset, bool atEnd) {
            const Ring& ring = chain.rings.at(run.at(position));
            const QUuid tripId = tripOfRing(run.at(atEnd ? position : position + 1));
            if (cap == EndCap::Dome) {
                addDome(writer, tripId, ring, firstVertex.at(position), atEnd,
                        atEnd ? chain.endDomeHeightMeters : chain.startDomeHeightMeters);
                return;
            }
            const double offset = ring.frame.inclinationWeight > kSteepWeight ? pitOffset : 0.0;
            const QVector3D outward = atEnd ? ring.frame.tangent : -ring.frame.tangent;
            const quint32 center = writer.addVertex(ring.center + outward * static_cast<float>(offset));
            writer.addFan(tripId, center, firstVertex.at(position), resolution, atEnd);
        };
        const bool isChainStart = run.first() == 0;
        const bool isChainEnd = run.last() == lastRing;
        addCap(0, isChainStart ? chain.startCap : EndCap::Fan, isChainStart ? chain.startCapOffsetMeters : 0.0, false);
        addCap(run.size() - 1, isChainEnd ? chain.endCap : EndCap::Fan, isChainEnd ? chain.endCapOffsetMeters : 0.0, true);
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
    debug.chains = buildChains(debug.legs);
    classifyChainEnds(debug.legs, debug.chains);

    const StationLegDirections directions = stationLegDirections(debug.legs);
    const StationRecords records = stationRecords(debug.legs);
    MeshWriter writer(debug.mesh);
    for (Chain& chain : debug.chains) {
        buildRings(debug.legs, directions, records, chain, options, debug.warnings);
        placeChainVertices(chain);
        addChainMesh(writer, debug.legs, chain, debug.warnings);
    }
    writer.finish();
    debug.mesh.warnings = debug.warnings;
    return debug;
}
