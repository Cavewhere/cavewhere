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
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr int kMinFilledBins = 4;
constexpr int kTwistDistributionSamples = 4;
constexpr double kMinSampleWeight = 0.05;
constexpr double kSteepWeight = 0.5;
constexpr double kMinSampleSpacingMeters = 0.01;
constexpr double kAxisEpsilon = 1e-9;
constexpr float kMinProjectedLength = 1e-6f;
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
        const auto isPositioned = [&](const cwStation& station) {
            return station.isValid() && positions.hasPosition(station.name());
        };

        for (const cwTripData& trip : cave.trips) {
            const cwUnits::LengthUnit unit = trip.calibrations.distanceUnit();
            for (int chunkIndex = 0; chunkIndex < trip.chunks.size(); ++chunkIndex) {
                const cwSurveyChunkData& chunk = trip.chunks.at(chunkIndex);

                QVector<cwStationLrud> lruds;
                lruds.reserve(chunk.stations.size());
                for (const cwStation& station : chunk.stations) {
                    lruds.append(stationLrud(station, unit, trip.id, warnings));
                }

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
                        lruds.at(shotIndex),
                        lruds.at(shotIndex + 1)
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

using Frame = cwPassageTubeBuilder::Frame;
using Ring = cwPassageTubeBuilder::Ring;
using Options = cwPassageTubeBuilder::Options;

// One place along a chain where a ring is fitted. lruds are already oriented
// to the chain's direction of travel (left and right swapped on legs walked
// backwards).
struct ChainSample {
    QVector3D position;
    QVector3D tangent;
    double distanceAlongChain = 0.0;
    QVector<cwStationLrud> lruds;
};

cwStationLrud swappedLeftRight(const cwStationLrud& lrud)
{
    return {lrud.right, lrud.left, lrud.up, lrud.down};
}

cwStationLrud lerpLrud(const cwStationLrud& a, const cwStationLrud& b, double t)
{
    const auto lerp = [t](const std::optional<double>& x, const std::optional<double>& y) -> std::optional<double> {
        if (x && y) {
            return *x + (*y - *x) * t;
        }
        return std::nullopt;
    };
    return {lerp(a.left, b.left), lerp(a.right, b.right), lerp(a.up, b.up), lerp(a.down, b.down)};
}

// A chain leg as walked: start and end in travel order, LRUD oriented.
struct WalkedLeg {
    QVector3D start;
    QVector3D end;
    cwStationLrud startLrud;
    cwStationLrud endLrud;

    QVector3D direction() const { return (end - start).normalized(); }
};

WalkedLeg walkedLeg(const QVector<Leg>& legs, const ChainLeg& chainLeg)
{
    const Leg& leg = legs.at(chainLeg.legIndex);
    if (chainLeg.reversed) {
        return {leg.toPosition, leg.fromPosition, swappedLeftRight(leg.toLrud), swappedLeftRight(leg.fromLrud)};
    }
    return {leg.fromPosition, leg.toPosition, leg.fromLrud, leg.toLrud};
}

QVector3D bisector(const QVector3D& incoming, const QVector3D& outgoing)
{
    const QVector3D sum = incoming + outgoing;
    if (sum.lengthSquared() < kDegenerateLengthSquared) {
        return incoming;
    }
    return sum.normalized();
}

QVector<WalkedLeg> walkedLegs(const QVector<Leg>& legs, const Chain& chain)
{
    QVector<WalkedLeg> walked;
    walked.reserve(chain.legs.size());
    for (const ChainLeg& chainLeg : chain.legs) {
        walked.append(walkedLeg(legs, chainLeg));
    }
    return walked;
}

bool isLoop(const QVector<WalkedLeg>& walked)
{
    return walked.size() > 1 && walked.first().start == walked.last().end;
}

QVector<ChainSample> chainSamples(const QVector<WalkedLeg>& walked, const Options& options)
{
    const bool loop = isLoop(walked);
    const double spacing = std::max(options.sampleSpacingMeters, kMinSampleSpacingMeters);

    QVector<ChainSample> samples;
    double distance = 0.0;
    for (int legIndex = 0; legIndex < walked.size(); ++legIndex) {
        const WalkedLeg& leg = walked.at(legIndex);
        const QVector3D direction = leg.direction();

        if (legIndex == 0) {
            const QVector3D tangent = loop ? bisector(walked.last().direction(), direction) : direction;
            samples.append(ChainSample {leg.start, tangent, 0.0, {leg.startLrud}});
        }

        const double length = leg.start.distanceToPoint(leg.end);
        const int intervals = std::max(1, static_cast<int>(std::lround(length / spacing)));
        for (int i = 1; i < intervals; ++i) {
            const double t = static_cast<double>(i) / intervals;
            samples.append(ChainSample {
                leg.start + (leg.end - leg.start) * static_cast<float>(t),
                direction,
                distance + length * t,
                {lerpLrud(leg.startLrud, leg.endLrud, t)}
            });
        }

        distance += length;
        const bool isLast = legIndex == walked.size() - 1;
        QVector3D endTangent = direction;
        QVector<cwStationLrud> endLruds {leg.endLrud};
        if (isLast) {
            if (loop) {
                endTangent = samples.first().tangent;
            }
        } else {
            const WalkedLeg& next = walked.at(legIndex + 1);
            endTangent = bisector(direction, next.direction());
            endLruds.append(next.startLrud);
        }
        samples.append(ChainSample {leg.end, endTangent, distance, endLruds});
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
        for (int bin = 0; bin < resolution; ++bin) {
            const double theta = kTwoPi * bin / resolution;
            const double cosTheta = std::cos(theta);
            const double sinTheta = std::sin(theta);
            const bool usesHorizontal = std::abs(cosTheta) > kAxisEpsilon;
            const bool usesVertical = std::abs(sinTheta) > kAxisEpsilon;
            for (const cwStationLrud& lrud : sample.lruds) {
                const std::optional<double> a = cosTheta >= 0.0 ? lrud.right : lrud.left;
                const std::optional<double> b = sinTheta >= 0.0 ? lrud.up : lrud.down;
                if ((usesHorizontal && !a) || (usesVertical && !b)) {
                    continue;
                }
                const double radius = std::hypot(a.value_or(0.0) * cosTheta, b.value_or(0.0) * sinTheta);
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

// Level dead ends sit flush with the end station; pits extend by up or down.
double capOffset(const cwStationLrud& lrud, const QVector3D& outward, const Frame& endFrame)
{
    const std::optional<double> offset = outward.z() >= 0.0f ? lrud.up : lrud.down;
    return endFrame.inclinationWeight * offset.value_or(0.0);
}

void buildRings(const QVector<Leg>& legs, Chain& chain, const Options& options, QVector<Warning>& warnings)
{
    const QVector<WalkedLeg> walked = walkedLegs(legs, chain);
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

    const ChainLeg& firstLeg = chain.legs.first();
    if (fittedSamples.isEmpty()) {
        const Leg& leg = legs.at(firstLeg.legIndex);
        const QString station = firstLeg.reversed ? leg.to : leg.from;
        warnings.append(Warning {
            Warning::NoPassageData,
            leg.tripId,
            station,
            QStringLiteral("The passage from station %1 has no LRUD; the tube uses the default radius.").arg(station)
        });
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
        Ring ring {samples.at(i).position, fits.at(i).radii, frames.at(i), fits.at(i).measured};
        if (fits.at(i).filledBins < kMinFilledBins) {
            ring.measured = QVector<bool>(resolution, false);
            ring.radii = fittedSamples.isEmpty()
                             ? QVector<double>(resolution, options.defaultRadiusMeters)
                             : fits.at(nearestFitted(i)).radii;
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
}

} // namespace

cwPassageTubeMesh cwPassageTubeBuilder::build(const cwCavingRegionData& region,
                                              const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                              const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                              const Options& options)
{
    cwPassageTubeMesh mesh;
    mesh.warnings = buildDebug(region, positionsByCave, splaysByCave, options).warnings;
    return mesh;
}

cwPassageTubeBuilder::Debug cwPassageTubeBuilder::buildDebug(const cwCavingRegionData& region,
                                                             const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                                             const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                                             const Options& options)
{
    Q_UNUSED(splaysByCave)

    Debug debug;
    debug.legs = extractLegs(region, positionsByCave, debug.warnings);
    debug.chains = buildChains(debug.legs);
    for (Chain& chain : debug.chains) {
        buildRings(debug.legs, chain, options, debug.warnings);
    }
    return debug;
}
