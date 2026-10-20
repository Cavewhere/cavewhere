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

constexpr double kMinSampleSpacingMeters = 0.01;
// Two shots between the same stations whose ends lie this close are one
// passage surveyed twice.
constexpr double kRepeatedShotToleranceMeters = 0.1;
// A ring without readings borrows from its own trip within this many legs
// before a nearer ring of another trip.
constexpr int kSameTripBorrowLegs = 2;
constexpr double kOnAxisMeters = 1e-6;
constexpr double kQuarterTurn = std::numbers::pi / 2.0;
constexpr double kHalfTurnDegrees = 180.0;
constexpr QVector3D kWorldUp(0.0f, 0.0f, 1.0f);

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

// The plan direction the shot's compass reads, calibrated and turned by the
// trip's manual declination: the frontsight, else the backsight. A
// corrected frontsight or an uncorrected backsight reads the reverse way.
std::optional<QVector3D> shotFacing(const cwShot& shot, const cwTripCalibrationData& calibrations)
{
    const auto bearing = [&]() -> std::optional<double> {
        if (shot.compass().state() == cwCompassReading::State::Valid) {
            const double turn = calibrations.hasCorrectedCompassFrontsight() ? kHalfTurnDegrees : 0.0;
            return shot.compass().toDouble() + calibrations.frontCompassCalibration() + turn;
        }
        if (shot.backCompass().state() == cwCompassReading::State::Valid) {
            const double turn = calibrations.hasCorrectedCompassBacksight() ? 0.0 : kHalfTurnDegrees;
            return shot.backCompass().toDouble() + calibrations.backCompassCalibration() + turn;
        }
        return std::nullopt;
    }();
    if (bearing) {
        const double declination = calibrations.autoDeclination() ? 0.0 : calibrations.declinationManual();
        const double radians = qDegreesToRadians(*bearing + declination);
        return QVector3D(static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)), 0.0f);
    }
    return std::nullopt;
}

bool isPositioned(const cwStation& station, const cwStationPositionLookup& positions)
{
    return station.isValid() && positions.hasPosition(station.name());
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
// chunk, collapse into the first: it keeps its trip, and each of
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

QHash<QUuid, QString> tripNamesOf(const cwCavingRegionData& region)
{
    QHash<QUuid, QString> tripNames;
    for (const cwCaveData& cave : region.caves) {
        for (const cwTripData& trip : cave.trips) {
            tripNames.insert(trip.id, trip.name);
        }
    }
    return tripNames;
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
                        shotFacing(chunk.shots.at(shotIndex), trip.calibrations)
                    });
                }
            }
        }
    }
    return mergeRepeatedShots(legs, tripNamesOf(region), warnings);
}

using StationKey = QPair<QUuid, QString>;

// One chain per maximal run of consecutive shots of one chunk, in chunk
// order.
QVector<Chain> buildChains(const QVector<Leg>& legs)
{
    QVector<Chain> chains;
    for (int legIndex = 0; legIndex < legs.size(); ++legIndex) {
        const Leg& leg = legs.at(legIndex);
        const auto continuesRun = [&]() {
            if (chains.isEmpty()) {
                return false;
            }
            const Leg& previous = legs.at(chains.constLast().legs.constLast().legIndex);
            return previous.tripId == leg.tripId && previous.chunkIndex == leg.chunkIndex && previous.shotIndex + 1 == leg.shotIndex;
        };
        if (!continuesRun()) {
            chains.append(Chain());
        }
        chains.last().legs.append(ChainLeg {legIndex, false});
    }
    return chains;
}

using Ring = cwPassageTubeBuilder::Ring;
using Section = cwPassageTubeBuilder::Section;
using Options = cwPassageTubeBuilder::Options;
using EndCap = cwPassageTubeBuilder::EndCap;
using LegPad = cwPassageTubeBuilder::LegPad;

QVector3D inPlan(const QVector3D& v)
{
    return QVector3D(v.x(), v.y(), 0.0f);
}

// The level direction square to a vertical ring whose right is right, so
// that right × tangent is plumb up.
QVector3D levelTangent(const QVector3D& right)
{
    return QVector3D::crossProduct(kWorldUp, right).normalized();
}

// A chain leg as walked: start and end in travel order, with the record the
// leg's own chunk took at each.
struct WalkedLeg {
    QVector3D start;
    QVector3D end;
    cwStationLrud startLrud;
    cwStationLrud endLrud;
    QUuid tripId;
};

WalkedLeg walkedLeg(const QVector<Leg>& legs, const ChainLeg& chainLeg)
{
    const Leg& leg = legs.at(chainLeg.legIndex);
    if (chainLeg.reversed) {
        return {leg.toPosition, leg.fromPosition, leg.toLrud, leg.fromLrud, leg.tripId};
    }
    return {leg.fromPosition, leg.toPosition, leg.fromLrud, leg.toLrud, leg.tripId};
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

// The section's points: the quadrant ellipse of walls around center at each
// bin's eccentric anomaly, left and right along right, up and down plumb.
QVector<QVector3D> sectionPoints(const QVector3D& center, const QVector3D& right, const cwStationLrud& walls, int resolution)
{
    QVector<QVector3D> points;
    points.reserve(resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        const double t = binParameter(bin, resolution);
        const auto [horizontal, vertical] = quadrantReadings(walls, t);
        points.append(center
                      + right * static_cast<float>(*horizontal * std::cos(t))
                      + kWorldUp * static_cast<float>(*vertical * std::sin(t)));
    }
    return points;
}

// One end of a chain leg: the station, its right (clockwise of its facing),
// the record the leg's chunk took there, and the walls the section draws,
// borrowed when the record reads too little.
struct LegEnd {
    QVector3D station;
    double distanceAlongChain = 0.0;
    int chainLeg = 0;
    QUuid tripId;
    QVector3D right;
    cwStationLrud record;
    std::optional<cwStationLrud> walls;
    bool isBorrowed = false;

    // slope is |dz| / length of the leg this end belongs to: a plumb leg pads
    // its full mean width, a level one stands on its station.
    double pad(double slope) const { return slope * *meanReading(walls->left, walls->right); }
};

// A leg end's section around center as a ring's vertices and right, with
// the bins its readings measured.
struct EndSection {
    QVector3D station;
    QVector3D center;
    QVector<QVector3D> points;
    QVector3D right;
    QVector<bool> measured;
    std::optional<cwStationLrud> readings;
};

EndSection endSection(const LegEnd& end, const QVector3D& center, int resolution)
{
    return EndSection {end.station,
                       center,
                       sectionPoints(center, end.right, *end.walls, resolution),
                       end.right,
                       end.isBorrowed ? QVector<bool>(resolution, false) : measuredBins(end.record, resolution),
                       end.isBorrowed ? std::nullopt : end.walls};
}

// A shaft leg's section at one of its stations: a ring square to the leg's
// axis around center, left and right along planRight, and mean(L, R) along
// planRight × axis both ways, so a plumb shaft is as deep along the shot as it
// is wide. Both ends share planRight so their bins line up; a station facing
// against it has its readings swapped onto the side they were read on. Only
// the side readings are measured; the ring carries no Section, since U and D
// are the shaft's run, not its walls.
EndSection shaftSection(const LegEnd& end, const QVector3D& center, const QVector3D& axis, const QVector3D& planRight, int resolution)
{
    const bool facesPlanRight = QVector3D::dotProduct(planRight, end.right) >= 0.0f;
    const auto alongPlanRight = [facesPlanRight](const cwStationLrud& lrud) {
        return facesPlanRight ? lrud
                              : cwStationLrud {lrud.right, lrud.left, lrud.up, lrud.down};
    };
    const cwStationLrud walls = alongPlanRight(*end.walls);
    const cwStationLrud record = alongPlanRight(end.record);
    const QVector3D across = QVector3D::crossProduct(planRight, axis).normalized();
    const double depth = *meanReading(walls.left, walls.right);
    const cwStationLrud sides {record.left, record.right, std::nullopt, std::nullopt};
    QVector<QVector3D> points;
    points.reserve(resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        const double t = binParameter(bin, resolution);
        const std::optional<double> horizontal = quadrantReadings(walls, t).first;
        points.append(center
                      + planRight * static_cast<float>(*horizontal * std::cos(t))
                      + across * static_cast<float>(depth * std::sin(t)));
    }
    return EndSection {end.station,
                       center,
                       points,
                       planRight,
                       end.isBorrowed ? QVector<bool>(resolution, false) : measuredBins(sides, resolution),
                       std::nullopt};
}

// How far a shaft's dome stands past its end section along outward.
double shaftCapHeight(cwPassageTubeBuilder::ShaftCapHeight height, const cwStationLrud& walls, const QVector3D& outward)
{
    switch (height) {
    case cwPassageTubeBuilder::ShaftCapHeight::Reading:
        return outward.z() >= 0.0f ? *walls.up : *walls.down;
    case cwPassageTubeBuilder::ShaftCapHeight::MeanWidth:
        return *meanReading(walls.left, walls.right);
    case cwPassageTubeBuilder::ShaftCapHeight::Flat:
        break;
    }
    return 0.0;
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
// first from its own trip within kSameTripBorrowLegs legs. ends run in chain
// order, each leg's start before its end. Without any, the sections are
// round at the default radius and the chain warns.
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
}

// Lofts each leg between its two end rings (redesign step 5). An end ring is
// its station's section: the quadrant ellipse of the walls, left and right
// level along the station's right (clockwise of its facing, chainFacings),
// up and down plumb, standing e·pad off the station along the leg's plan
// run (LegPad). A leg's plan direction is its own, else the chain's incoming
// one, else the outgoing one, else the station's facing. The rings between
// are the point-wise lerps of the two, evenly between their centers.
//
// Two legs whose rings both stand on the station between them share its
// ring, from the record of the leg arriving there; elsewhere the chain
// splits there into pieces, each its own chain closed by fans, and a loop
// opens at its first split.
QVector<Chain> buildRings(const QVector<Leg>& legs, const Chain& chain, const Options& options, QVector<Warning>& warnings)
{
    QVector<WalkedLeg> walked;
    walked.reserve(chain.legs.size());
    for (const ChainLeg& chainLeg : chain.legs) {
        walked.append(walkedLeg(legs, chainLeg));
    }
    const bool loop = chain.startCap == EndCap::None;
    if (loop) {
        walked.last().endLrud = walked.first().startLrud;
    }
    const int resolution = std::max(options.ringResolution, 1);
    const double spacing = std::max(options.sampleSpacingMeters, kMinSampleSpacingMeters);
    const int legCount = walked.size();
    const QVector<QVector3D> facings = chainFacings(legs, chain);

    QVector<LegEnd> ends;
    ends.reserve(2 * legCount);
    double distance = 0.0;
    for (int i = 0; i < legCount; ++i) {
        const WalkedLeg& leg = walked.at(i);
        ends.append(LegEnd {leg.start, distance, i, leg.tripId, clockwiseOf(facings.at(i)), leg.startLrud, drawnWalls(leg.startLrud)});
        distance += leg.start.distanceToPoint(leg.end);
        ends.append(LegEnd {leg.end, distance, i, leg.tripId, clockwiseOf(facings.at(i + 1)), leg.endLrud, drawnWalls(leg.endLrud)});
    }
    borrowWalls(ends, legs, chain, options, warnings);

    QVector<QVector3D> plans;
    plans.reserve(legCount);
    for (const WalkedLeg& leg : std::as_const(walked)) {
        plans.append(inPlan(leg.end - leg.start));
    }
    const auto hasPlanRun = [&](int i) {
        return plans.at(i).length() >= kMinPlanFacingLength;
    };
    const auto planDirection = [&](int i) {
        if (hasPlanRun(i)) {
            return plans.at(i).normalized();
        }
        for (int k = 1; k < legCount; ++k) {
            const int before = loop ? (i - k + legCount) % legCount : i - k;
            if (before >= 0 && hasPlanRun(before)) {
                return plans.at(before).normalized();
            }
        }
        for (int after = i + 1; after < legCount; ++after) {
            if (hasPlanRun(after)) {
                return plans.at(after).normalized();
            }
        }
        return facings.at(i);
    };

    QVector<LegPad> pads;
    QVector<QVector3D> startCenters;
    QVector<QVector3D> endCenters;
    pads.reserve(legCount);
    startCenters.reserve(legCount);
    endCenters.reserve(legCount);
    for (int i = 0; i < legCount; ++i) {
        const QVector3D delta = walked.at(i).end - walked.at(i).start;
        const double length = delta.length();
        const double rise = std::abs(delta.z());
        const double run = plans.at(i).length();
        const double slope = length > 0.0 ? rise / length : 0.0;
        const cwStationLrud& startWalls = *ends.at(2 * i).walls;
        const cwStationLrud& endWalls = *ends.at(2 * i + 1).walls;
        const double width = (*startWalls.left + *startWalls.right + *endWalls.left + *endWalls.right) / 2.0;
        const double height = (*startWalls.up + *startWalls.down + *endWalls.up + *endWalls.down) / 2.0;
        const double riseRatio = height > 0.0 ? rise / height : 0.0;
        const double thinness = rise > 0.0 && height > 0.0
                                    ? (hasPlanRun(i) ? rise / run * width / height : std::numeric_limits<double>::infinity())
                                    : 0.0;
        const bool isShaft = options.shaftRingsSquare
                             && riseRatio > options.shaftRiseRatio
                             && thinness > options.shaftThinnessRatio;
        const double padStart = isShaft ? 0.0 : options.padScale * ends.at(2 * i).pad(slope);
        const double padEnd = isShaft ? 0.0 : options.padScale * ends.at(2 * i + 1).pad(slope);
        const double e = padStart + padEnd > 0.0 ? 1.0 - std::min(1.0, run / (padStart + padEnd)) : 0.0;
        pads.append(LegPad {padStart, padEnd, e, isShaft, riseRatio, thinness});
        const QVector3D along = planDirection(i);
        startCenters.append(walked.at(i).start - along * static_cast<float>(e * padStart));
        endCenters.append(walked.at(i).end + along * static_cast<float>(e * padEnd));
    }

    // Station s starts leg s; a loop's station 0 is its seam.
    const auto previousLeg = [&](int s) {
        return s > 0 ? s - 1 : legCount - 1;
    };
    QVector<int> splitStations;
    for (int s = loop ? 0 : 1; s < legCount; ++s) {
        const LegPad& arriving = pads.at(previousLeg(s));
        const LegPad& leaving = pads.at(s);
        const bool isShared = !arriving.isShaft && !leaving.isShaft
                              && arriving.e * arriving.end < kMinRingSpacingMeters
                              && leaving.e * leaving.start < kMinRingSpacingMeters;
        if (isShared) {
            endCenters[previousLeg(s)] = walked.at(s).start;
            startCenters[s] = walked.at(s).start;
        } else {
            splitStations.append(s);
        }
    }
    const bool staysLoop = loop && splitStations.isEmpty();
    const int firstLeg = loop && !splitStations.isEmpty() ? splitStations.first() : 0;

    QVector<QVector<int>> pieceLegs(1);
    for (int k = 0; k < legCount; ++k) {
        const int i = (firstLeg + k) % legCount;
        if (k > 0 && splitStations.contains(i)) {
            pieceLegs.append(QVector<int>());
        }
        pieceLegs.last().append(i);
    }

    QVector<Chain> pieces;
    for (const QVector<int>& order : std::as_const(pieceLegs)) {
        Chain piece;
        piece.startCap = staysLoop ? EndCap::None : EndCap::Fan;
        piece.endCap = piece.startCap;

        // A ring closer than kMinRingSpacingMeters to the one before merges
        // into it.
        const auto appendRing = [&](const QVector3D& center,
                                    const QVector<QVector3D>& points,
                                    const QVector3D& right,
                                    const QVector3D& tangent,
                                    int chainLeg,
                                    const QVector<bool>& measured) -> Ring* {
            if (!piece.rings.isEmpty()) {
                const QVector3D& previous = piece.rings.constLast().center;
                if (previous != center && previous.distanceToPoint(center) < kMinRingSpacingMeters) {
                    return nullptr;
                }
            }
            Ring ring;
            ring.center = center;
            ring.chainLeg = chainLeg;
            ring.measured = measured;
            cwPassageTubeGeometry::setRingVertices(ring, points, tangent, right);
            piece.rings.append(ring);
            return &piece.rings.last();
        };
        const auto appendSection = [&](const EndSection& end, const QVector3D& tangent, int chainLeg) {
            Ring* ring = appendRing(end.center, end.points, end.right, tangent, chainLeg, end.measured);
            if (ring != nullptr && end.readings) {
                ring->section = Section {*end.readings, ring->radii, end.station, end.center};
            }
        };

        piece.facings.append(facings.at(order.constFirst()));
        for (int k = 0; k < order.size(); ++k) {
            const int i = order.at(k);
            piece.legs.append(chain.legs.at(i));
            piece.legPads.append(pads.at(i));
            piece.facings.append(facings.at(i + 1));

            const bool sharesStart = k > 0;
            const bool isSeamEnd = staysLoop && i == legCount - 1;
            const LegEnd& startEnd = sharesStart ? ends.at(2 * previousLeg(i) + 1) : ends.at(2 * i);
            const LegEnd& endEnd = isSeamEnd ? ends.first() : ends.at(2 * i + 1);
            const bool isShaft = pads.at(i).isShaft;
            const QVector3D axis = (walked.at(i).end - walked.at(i).start).normalized();
            const QVector3D shaftRight = clockwiseOf(planDirection(i));
            const EndSection start = isShaft ? shaftSection(startEnd, startCenters.at(i), axis, shaftRight, resolution)
                                             : endSection(startEnd, startCenters.at(i), resolution);
            const EndSection end = isShaft ? shaftSection(endEnd, endCenters.at(i), axis, shaftRight, resolution)
                                           : endSection(endEnd, endCenters.at(i), resolution);

            // A shaft's dome: rings shrinking from the end section toward an
            // apex past it along outward, the apex ring closed by the fan.
            // Appended apex first at the start, apex last at the end.
            const auto appendDome = [&](const EndSection& section, const LegEnd& legEnd, const QVector3D& outward, bool apexFirst) {
                const double capHeight = shaftCapHeight(options.shaftCapHeight, *legEnd.walls, outward);
                if (capHeight <= 0.0) {
                    return;
                }
                const bool isRound = options.shaftCapShape == cwPassageTubeBuilder::ShaftCapShape::Round;
                const int domeSteps = cwPassageTubeBuilder::kDomeRings + 1;
                for (int step = 1; step < domeSteps; ++step) {
                    const int pastSection = apexFirst ? domeSteps - step : step;
                    const double fraction = static_cast<double>(pastSection) / domeSteps;
                    const double scale = isRound ? std::cos(fraction * kQuarterTurn) : 1.0 - fraction;
                    const double offset = capHeight * (isRound ? std::sin(fraction * kQuarterTurn) : fraction);
                    const QVector3D center = section.center + outward * static_cast<float>(offset);
                    QVector<QVector3D> points;
                    points.reserve(resolution);
                    for (const QVector3D& point : section.points) {
                        points.append(center + (point - section.center) * static_cast<float>(scale));
                    }
                    appendRing(center, points, section.right, axis, k, QVector<bool>(resolution, false));
                }
            };
            const auto tangentOf = [&](const QVector3D& right) {
                return isShaft ? axis : levelTangent(right);
            };

            if (!sharesStart) {
                if (isShaft) {
                    appendDome(start, startEnd, -axis, true);
                }
                appendSection(start, tangentOf(start.right), k);
            }

            const double length = start.center.distanceToPoint(end.center);
            const int intervals = std::max(1, static_cast<int>(std::lround(length / spacing)));
            for (int j = 1; j < intervals; ++j) {
                const float t = static_cast<float>(j) / static_cast<float>(intervals);
                if ((1.0 - t) * length < kMinRingSpacingMeters) {
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
                const QVector3D lerpedRight = start.right + (end.right - start.right) * t;
                const QVector3D right = lerpedRight.length() >= kMinPlanFacingLength ? lerpedRight.normalized() : start.right;
                appendRing(start.center + (end.center - start.center) * t, points, right, tangentOf(right), k, measured);
            }

            appendSection(end, tangentOf(end.right), k);
            if (isShaft) {
                appendDome(end, endEnd, axis, false);
            }
        }
        pieces.append(piece);
    }
    return pieces;
}

StationKey canonicalStation(const QUuid& caveId, const QString& station)
{
    return {caveId, cwStation::canonicalKey(station)};
}

// A chain that ends at the station it starts from is a loop and has no
// caps; every other chain end is a fan.
void classifyChainEnds(const QVector<Leg>& legs, QVector<Chain>& chains)
{
    for (Chain& chain : chains) {
        const ChainLeg& first = chain.legs.first();
        const ChainLeg& last = chain.legs.last();
        const bool isLoop = chain.legs.size() > 1
                            && cwStation::canonicalKey(chainStartStation(legs, first)) == cwStation::canonicalKey(chainEndStation(legs, last));
        chain.startCap = isLoop ? EndCap::None : EndCap::Fan;
        chain.endCap = chain.startCap;
    }
}

// Warns once per station that two chains reach, where their records read
// the same side wall, the side whose right points the same way in plan, more
// than kReadingConflictRatio apart.
void warnReadingConflicts(const cwCavingRegionData& region, const QVector<Leg>& legs, const QVector<Chain>& chains, QVector<Warning>& warnings)
{
    const QHash<QUuid, QString> tripNames = tripNamesOf(region);

    struct ChainRecord {
        int chain = -1;
        QUuid tripId;
        QString station;
        cwStationLrud lrud;
        QVector3D right;
    };
    QHash<StationKey, QVector<ChainRecord>> recordsByStation;
    QVector<StationKey> stationOrder;
    for (int c = 0; c < chains.size(); ++c) {
        const Chain& chain = chains.at(c);
        const QVector<QVector3D> facings = chainFacings(legs, chain);
        for (int s = 0; s <= chain.legs.size(); ++s) {
            const bool isEnd = s == chain.legs.size();
            const ChainLeg& chainLeg = chain.legs.at(isEnd ? s - 1 : s);
            const WalkedLeg leg = walkedLeg(legs, chainLeg);
            const QString& station = isEnd ? chainEndStation(legs, chainLeg) : chainStartStation(legs, chainLeg);
            const StationKey key = canonicalStation(legs.at(chainLeg.legIndex).caveId, station);
            QVector<ChainRecord>& records = recordsByStation[key];
            const bool hasChainRecord = std::any_of(records.cbegin(), records.cend(), [&](const ChainRecord& record) {
                return record.chain == c;
            });
            if (hasChainRecord) {
                continue;
            }
            if (records.isEmpty()) {
                stationOrder.append(key);
            }
            records.append({c, leg.tripId, station, isEnd ? leg.endLrud : leg.startLrud, clockwiseOf(facings.at(s))});
        }
    }

    const auto conflict = [](const std::optional<double>& a, const std::optional<double>& b) {
        constexpr double ratio = cwPassageTubeBuilder::kReadingConflictRatio;
        return a && b && (*a > ratio * *b || *b > ratio * *a);
    };
    for (const StationKey& key : std::as_const(stationOrder)) {
        const QVector<ChainRecord>& records = recordsByStation.value(key);
        const auto warnFirstConflict = [&]() {
            for (int i = 0; i < records.size(); ++i) {
                for (int k = i + 1; k < records.size(); ++k) {
                    const ChainRecord& a = records.at(i);
                    const ChainRecord& b = records.at(k);
                    const bool isSameWay = QVector3D::dotProduct(a.right, b.right) >= 0.0f;
                    const std::array<std::pair<std::optional<double>, std::optional<double>>, 2> sides {{
                        {a.lrud.left, isSameWay ? b.lrud.left : b.lrud.right},
                        {a.lrud.right, isSameWay ? b.lrud.right : b.lrud.left},
                    }};
                    for (const auto& [readingA, readingB] : sides) {
                        if (conflict(readingA, readingB)) {
                            warnings.append(Warning {
                                Warning::JunctionReadingConflict,
                                b.tripId,
                                a.station,
                                QStringLiteral("Station %1 reads a side wall at %2 m in trip %3 and %4 m in trip %5; the tube draws both.")
                                    .arg(a.station)
                                    .arg(*readingA)
                                    .arg(tripNames.value(a.tripId))
                                    .arg(*readingB)
                                    .arg(tripNames.value(b.tripId))
                            });
                            return;
                        }
                    }
                }
            }
        };
        warnFirstConflict();
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

    // Each tube's end fans are caps: one entry per tube and end, -1 for a
    // loop's.
    QVector<std::array<int, 2>> capOfEnd;
    capOfEnd.reserve(tubes.size());
    for (const Union::ChainTube& tube : tubes) {
        const Chain& chain = debug.chains.at(tube.chain);
        const auto addCap = [&](int ring) {
            if (tube.isLoop) {
                return -1;
            }
            debug.caps.append({tube.chain, caveOf(debug.legs, chain), stationOfRing(debug.legs, chain, ring)});
            return static_cast<int>(debug.caps.size()) - 1;
        };
        capOfEnd.append({addCap(tube.rings.first()), addCap(tube.rings.last())});
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
    QVector<Chain> runs = buildChains(debug.legs);
    classifyChainEnds(debug.legs, runs);
    warnReadingConflicts(region, debug.legs, runs, debug.warnings);
    for (const Chain& run : std::as_const(runs)) {
        for (Chain& piece : buildRings(debug.legs, run, options, debug.warnings)) {
            placeChainVertices(piece);
            debug.chains.append(piece);
        }
    }
    return debug;
}
