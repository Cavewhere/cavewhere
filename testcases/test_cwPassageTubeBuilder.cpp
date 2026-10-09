/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

//Our includes
#include "cwPassageTubeBuilder.h"

//Qt includes
#include <QStringList>

//Std includes
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using Builder = cwPassageTubeBuilder;

struct StationInput {
    QString name;
    std::optional<QVector3D> position;
    QStringList lrud = {QString(), QString(), QString(), QString()};
};

// One cave, one trip; each inner list is a chunk of stations surveyed in order.
struct SurveyInput {
    QList<QList<StationInput>> chunks;
    cwUnits::LengthUnit distanceUnit = cwUnits::Meters;
};

struct BuilderInput {
    cwCavingRegionData region;
    QHash<QUuid, cwStationPositionLookup> positionsByCave;
};

// One cave with a trip per survey; station positions are shared by name.
BuilderInput makeTripsInput(const QList<SurveyInput>& surveys)
{
    cwCaveData cave;
    cave.id = QUuid::createUuid();

    cwStationPositionLookup positions;
    for (const SurveyInput& survey : surveys) {
        cwTripData trip;
        trip.id = QUuid::createUuid();
        trip.calibrations.setDistanceUnit(survey.distanceUnit);

        for (const QList<StationInput>& stations : survey.chunks) {
            cwSurveyChunkData chunk;
            chunk.id = QUuid::createUuid();
            for (const StationInput& input : stations) {
                cwStation station(input.name);
                station.setLeft(cwDistanceReading(input.lrud.at(0)));
                station.setRight(cwDistanceReading(input.lrud.at(1)));
                station.setUp(cwDistanceReading(input.lrud.at(2)));
                station.setDown(cwDistanceReading(input.lrud.at(3)));
                chunk.stations.append(station);
                if (input.position) {
                    positions.setPosition(input.name, *input.position);
                }
            }
            for (int i = 1; i < stations.size(); ++i) {
                chunk.shots.append(cwShot());
            }
            trip.chunks.append(chunk);
        }
        cave.trips.append(trip);
    }

    BuilderInput input;
    input.region.caves.append(cave);
    input.positionsByCave.insert(cave.id, positions);
    return input;
}

BuilderInput makeInput(const SurveyInput& survey)
{
    return makeTripsInput({survey});
}

Builder::Debug buildDebug(const BuilderInput& input)
{
    return Builder::buildDebug(input.region, input.positionsByCave, {}, Builder::Options());
}

QStringList lrud(double left, double right, double up, double down)
{
    return {QString::number(left), QString::number(right), QString::number(up), QString::number(down)};
}

// The ring bin at angle degrees from right toward up, for the default 16 bins.
int binAt(int degrees)
{
    return degrees * Builder::Options().ringResolution / 360;
}

const Builder::Ring& ringAt(const Builder::Chain& chain, const QVector3D& center)
{
    const auto it = std::find_if(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& ring) {
        return ring.center.distanceToPoint(center) < 1e-4f;
    });
    REQUIRE(it != chain.rings.cend());
    return *it;
}

bool hasMeasuredBin(const Builder::Ring& ring)
{
    return std::find(ring.measured.cbegin(), ring.measured.cend(), true) != ring.measured.cend();
}

bool isFinite(const QVector3D& v)
{
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

// Consecutive rings keep their orientation: the twist between neighbors stays under 90°.
void checkFramesContinuous(const Builder::Chain& chain)
{
    for (int i = 0; i < chain.rings.size(); ++i) {
        const Builder::Frame& frame = chain.rings.at(i).frame;
        INFO("ring " << i);
        REQUIRE(isFinite(frame.up));
        REQUIRE(isFinite(frame.right));
        CHECK(frame.up.length() == Catch::Approx(1.0).margin(1e-4));
        CHECK(std::abs(QVector3D::dotProduct(frame.up, frame.tangent)) < 1e-4f);
        if (i > 0) {
            CHECK(QVector3D::dotProduct(frame.right, chain.rings.at(i - 1).frame.right) > 0.0f);
        }
    }
}

void checkVectorNear(const QVector3D& actual, const QVector3D& expected)
{
    INFO("actual (" << actual.x() << ", " << actual.y() << ", " << actual.z() << ")");
    CHECK(actual.distanceToPoint(expected) < 1e-4f);
}

QString chainStart(const Builder::Debug& debug, const Builder::ChainLeg& chainLeg)
{
    const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
    return chainLeg.reversed ? leg.to : leg.from;
}

QString chainEnd(const Builder::Debug& debug, const Builder::ChainLeg& chainLeg)
{
    const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
    return chainLeg.reversed ? leg.from : leg.to;
}

// Every leg in exactly one chain, and each chain walks leg to leg without a gap.
void checkChainsCoverLegs(const Builder::Debug& debug)
{
    QVector<int> useCount(debug.legs.size(), 0);
    for (const Builder::Chain& chain : debug.chains) {
        REQUIRE(!chain.legs.isEmpty());
        for (int i = 0; i < chain.legs.size(); ++i) {
            const Builder::ChainLeg& chainLeg = chain.legs.at(i);
            REQUIRE(chainLeg.legIndex >= 0);
            REQUIRE(chainLeg.legIndex < debug.legs.size());
            useCount[chainLeg.legIndex]++;
            if (i > 0) {
                CHECK(chainStart(debug, chainLeg) == chainEnd(debug, chain.legs.at(i - 1)));
            }
        }
    }
    for (int count : std::as_const(useCount)) {
        CHECK(count == 1);
    }
}

QStringList chainStations(const Builder::Debug& debug, const Builder::Chain& chain)
{
    QStringList stations {chainStart(debug, chain.legs.first())};
    for (const Builder::ChainLeg& chainLeg : chain.legs) {
        stations.append(chainEnd(debug, chainLeg));
    }
    return stations;
}

// Every directed edge of the triangles in [first, first + count) has exactly
// one partner running the other way: the surface is closed and consistently
// wound.
bool isClosedSurface(const QVector<quint32>& indices, int first, int count)
{
    QHash<std::pair<quint32, quint32>, int> directedEdges;
    for (int i = first; i < first + count; i += 3) {
        for (int corner = 0; corner < 3; ++corner) {
            const quint32 from = indices.at(i + corner);
            const quint32 to = indices.at(i + (corner + 1) % 3);
            directedEdges[{from, to}]++;
        }
    }
    for (auto it = directedEdges.cbegin(); it != directedEdges.cend(); ++it) {
        if (it.value() != 1 || directedEdges.value({it.key().second, it.key().first}) != 1) {
            return false;
        }
    }
    return !directedEdges.isEmpty();
}

bool isClosedSurface(const QVector<quint32>& indices)
{
    return isClosedSurface(indices, 0, indices.size());
}

// Positive when the triangles wind counter-clockwise seen from outside.
double signedVolume(const cwPassageTubeMesh& mesh)
{
    double volume = 0.0;
    for (int i = 0; i < mesh.indices.size(); i += 3) {
        const QVector3D a = mesh.positions.at(mesh.indices.at(i));
        const QVector3D b = mesh.positions.at(mesh.indices.at(i + 1));
        const QVector3D c = mesh.positions.at(mesh.indices.at(i + 2));
        volume += QVector3D::dotProduct(a, QVector3D::crossProduct(b, c)) / 6.0;
    }
    return volume;
}

void checkNoFoldBack(const Builder::Ring& previous, const Builder::Ring& ring)
{
    for (const QVector3D& vertex : ring.vertices) {
        CHECK(QVector3D::dotProduct(vertex - previous.center, previous.frame.tangent) >= -1e-4f);
    }
}

// A loop's last ring repeats its first, so the first ring is also checked
// against the ring before the last.
void checkNoFoldBack(const Builder::Chain& chain)
{
    for (int i = 1; i < chain.rings.size(); ++i) {
        INFO("ring " << i);
        checkNoFoldBack(chain.rings.at(i - 1), chain.rings.at(i));
    }
    if (chain.startCap == Builder::EndCap::None && chain.rings.size() >= 3) {
        INFO("loop closure");
        checkNoFoldBack(chain.rings.at(chain.rings.size() - 2), chain.rings.first());
    }
}

bool hasPosition(const cwPassageTubeMesh& mesh, const QVector3D& position)
{
    return std::any_of(mesh.positions.cbegin(), mesh.positions.cend(), [&](const QVector3D& p) {
        return p.distanceToPoint(position) < 1e-4f;
    });
}

} // namespace

TEST_CASE("cwPassageTubeBuilder chains a straight line as one chain", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a1"), QVector3D(0, 0, 0)},
        {QStringLiteral("a2"), QVector3D(10, 0, 0)},
        {QStringLiteral("a3"), QVector3D(20, 0, 0)},
    }}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 2);
    CHECK(debug.legs.at(0).from == QStringLiteral("a1"));
    CHECK(debug.legs.at(0).to == QStringLiteral("a2"));
    CHECK(debug.legs.at(1).shotIndex == 1);
    CHECK(debug.legs.at(1).toPosition == QVector3D(20, 0, 0));

    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).legs.size() == 2);
    checkChainsCoverLegs(debug);
}

TEST_CASE("cwPassageTubeBuilder runs one chain straight through a T-junction", "[PassageTube][cwPassageTubeBuilder]")
{
    // The arm b-d is listed at the junction before b-c, so picking b-c
    // depends on the straightest-continuation rule rather than survey order.
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0)};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0)};
    const StationInput d {QStringLiteral("d"), QVector3D(10, 10, 0)};
    const BuilderInput input = makeInput({{{a, b}, {b, d}, {b, c}}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 3);
    REQUIRE(debug.chains.size() == 2);
    checkChainsCoverLegs(debug);

    const Builder::Chain& through = debug.chains.at(0);
    CHECK(chainStations(debug, through) == QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));

    const Builder::Chain& arm = debug.chains.at(1);
    REQUIRE(arm.legs.size() == 1);
    CHECK(arm.legs.at(0).reversed);
    CHECK(chainStations(debug, arm) == QStringList({QStringLiteral("d"), QStringLiteral("b")}));
}

TEST_CASE("cwPassageTubeBuilder runs one chain through a 4-way junction and ends the other arms there", "[PassageTube][cwPassageTubeBuilder]")
{
    const StationInput junction {QStringLiteral("j"), QVector3D(0, 0, 0)};
    const StationInput west {QStringLiteral("w"), QVector3D(-10, 0, 0)};
    const StationInput east {QStringLiteral("e"), QVector3D(10, 0, 0)};
    const StationInput north {QStringLiteral("n"), QVector3D(0, 10, 0)};
    const StationInput south {QStringLiteral("s"), QVector3D(0, -10, 0)};
    const BuilderInput input = makeInput({{{west, junction, east}, {north, junction}, {junction, south}}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 4);
    REQUIRE(debug.chains.size() == 3);
    checkChainsCoverLegs(debug);

    int throughChains = 0;
    int armChains = 0;
    for (const Builder::Chain& chain : debug.chains) {
        if (chain.legs.size() == 2) {
            ++throughChains;
            CHECK(chainStations(debug, chain) == QStringList({QStringLiteral("w"), QStringLiteral("j"), QStringLiteral("e")}));
        } else if (chain.legs.size() == 1) {
            ++armChains;
        }
    }
    CHECK(throughChains == 1);
    CHECK(armChains == 2);
}

TEST_CASE("cwPassageTubeBuilder walks a loop once and closes it", "[PassageTube][cwPassageTubeBuilder]")
{
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0)};
    const StationInput c {QStringLiteral("c"), QVector3D(5, 8, 0)};
    const BuilderInput input = makeInput({{{a, b, c, a}}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 3);
    REQUIRE(debug.chains.size() == 1);
    checkChainsCoverLegs(debug);

    const QStringList stations = chainStations(debug, debug.chains.at(0));
    CHECK(stations.first() == stations.last());
}

TEST_CASE("cwPassageTubeBuilder covers a loop hanging off a passage", "[PassageTube][cwPassageTubeBuilder]")
{
    const StationInput entrance {QStringLiteral("e"), QVector3D(-10, 0, 0)};
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 5, 0)};
    const StationInput c {QStringLiteral("c"), QVector3D(10, -5, 0)};
    const BuilderInput input = makeInput({{{entrance, a, b, c, a}}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 4);
    checkChainsCoverLegs(debug);
}

TEST_CASE("cwPassageTubeBuilder skips equated and unpositioned shots", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a1"), QVector3D(0, 0, 0)},
        {QStringLiteral("a2"), QVector3D(10, 0, 0)},
        {QStringLiteral("b1"), QVector3D(10, 0, 0)},
        {QStringLiteral("b2"), QVector3D(20, 0, 0)},
        {QStringLiteral("b3"), std::nullopt},
    }}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 2);
    CHECK(debug.legs.at(0).from == QStringLiteral("a1"));
    CHECK(debug.legs.at(1).from == QStringLiteral("b1"));
    CHECK(debug.legs.at(1).shotIndex == 2);
    checkChainsCoverLegs(debug);
}

TEST_CASE("cwPassageTubeBuilder reads LRUD in meters with the zero rules", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a1"), QVector3D(0, 0, 0), {QStringLiteral("10"), QString(), QStringLiteral("0"), QStringLiteral("5")}},
        {QStringLiteral("a2"), QVector3D(10, 0, 0), {QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")}},
        {QStringLiteral("a3"), QVector3D(20, 0, 0), {QString(), QStringLiteral("0"), QString(), QString()}},
    }}, cwUnits::Feet});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.legs.size() == 2);

    SECTION("Measured values convert from the trip's unit; empty stays empty; a lone zero gets the minimum radius") {
        const cwStationLrud& lrud = debug.legs.at(0).fromLrud;
        REQUIRE(lrud.left.has_value());
        CHECK(*lrud.left == Catch::Approx(3.048));
        CHECK_FALSE(lrud.right.has_value());
        REQUIRE(lrud.up.has_value());
        CHECK(*lrud.up == Catch::Approx(Builder::kMinRadiusMeters));
        REQUIRE(lrud.down.has_value());
        CHECK(*lrud.down == Catch::Approx(1.524));
    }

    SECTION("All zeros is no passage data") {
        CHECK_FALSE(debug.legs.at(0).toLrud.hasAny());
        CHECK_FALSE(debug.legs.at(1).fromLrud.hasAny());
    }

    SECTION("Zeros and empties together are no passage data") {
        CHECK_FALSE(debug.legs.at(1).toLrud.hasAny());
    }
}

TEST_CASE("cwPassageTubeBuilder fits a horizontal LRUD ring to the quadrant ellipse", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 2, 3, 4);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    // Two stations and nine samples between them at the default 1 m spacing.
    REQUIRE(chain.rings.size() == 11);
    checkFramesContinuous(chain);

    for (const Builder::Ring& ring : chain.rings) {
        REQUIRE(ring.radii.size() == Builder::Options().ringResolution);
        REQUIRE(ring.measured.size() == Builder::Options().ringResolution);
        CHECK(ring.frame.inclinationWeight == Catch::Approx(0.0));
        checkVectorNear(ring.frame.tangent, QVector3D(1, 0, 0));
        checkVectorNear(ring.frame.up, QVector3D(0, 0, 1));
        checkVectorNear(ring.frame.right, QVector3D(0, -1, 0));
        CHECK(ring.radii.at(binAt(0)) == Catch::Approx(2.0));
        CHECK(ring.radii.at(binAt(90)) == Catch::Approx(3.0));
        CHECK(ring.radii.at(binAt(180)) == Catch::Approx(1.0));
        CHECK(ring.radii.at(binAt(270)) == Catch::Approx(4.0));
        CHECK_FALSE(ring.measured.contains(false));
    }

    // Between the axes the ring follows the ellipse of the two adjacent dimensions.
    const double diagonal = std::hypot(2.0 * std::cos(std::numbers::pi / 4.0), 3.0 * std::sin(std::numbers::pi / 4.0));
    CHECK(chain.rings.at(0).radii.at(binAt(45)) == Catch::Approx(diagonal));
    CHECK(debug.warnings.isEmpty());

    // Level dead ends sit flush with their stations.
    CHECK(chain.startCapOffsetMeters == Catch::Approx(0.0));
    CHECK(chain.endCapOffsetMeters == Catch::Approx(0.0));
}

TEST_CASE("cwPassageTubeBuilder keeps frames continuous through a vertical pit", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(0, 10, 0), dimensions},
        {QStringLiteral("c"), QVector3D(0, 10, -10), dimensions},
        {QStringLiteral("d"), QVector3D(0, 20, -10), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    checkFramesContinuous(chain);

    const Builder::Ring& pit = ringAt(chain, QVector3D(0, 10, -5));
    CHECK(pit.frame.inclinationWeight == Catch::Approx(1.0));
    checkVectorNear(pit.frame.tangent, QVector3D(0, 0, -1));
    CHECK_FALSE(hasMeasuredBin(pit));
    for (double radius : pit.radii) {
        CHECK(radius == Catch::Approx(1.0));
    }

    const Builder::Ring& upper = ringAt(chain, QVector3D(0, 5, 0));
    checkVectorNear(upper.frame.up, QVector3D(0, 0, 1));
    CHECK_FALSE(upper.measured.contains(false));

    const Builder::Ring& lower = ringAt(chain, QVector3D(0, 15, -10));
    checkVectorNear(lower.frame.up, QVector3D(0, 0, 1));
}

TEST_CASE("cwPassageTubeBuilder unwinds the twist of a pit left on a new bearing", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(0, 10, 0)},
        {QStringLiteral("c"), QVector3D(0, 10, -10)},
        {QStringLiteral("d"), QVector3D(10, 10, -10)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    checkFramesContinuous(chain);

    // Well past the pit the passage is level again and up is world up.
    const Builder::Ring& exit = ringAt(chain, QVector3D(10, 10, -10));
    checkVectorNear(exit.frame.up, QVector3D(0, 0, 1));
    checkVectorNear(exit.frame.right, QVector3D(0, -1, 0));
}

TEST_CASE("cwPassageTubeBuilder keeps a chain that is vertical throughout finite", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(0, 0, -10)},
        {QStringLiteral("c"), QVector3D(0, 0, -20)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    checkFramesContinuous(chain);
    checkVectorNear(chain.rings.first().frame.up, QVector3D(0, 1, 0));
}

TEST_CASE("cwPassageTubeBuilder swaps left and right on a leg walked backwards", "[PassageTube][cwPassageTubeBuilder]")
{
    // The second chunk is surveyed west from c to b; the chain walks east.
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0)};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), lrud(1, 3, 2, 2)};
    const BuilderInput input = makeInput({{{a, b}, {c, b}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    REQUIRE(chain.legs.size() == 2);
    CHECK_FALSE(chain.legs.at(0).reversed);
    CHECK(chain.legs.at(1).reversed);

    const Builder::Ring& ring = ringAt(chain, QVector3D(20, 0, 0));
    checkVectorNear(ring.frame.right, QVector3D(0, -1, 0));
    CHECK(ring.radii.at(binAt(0)) == Catch::Approx(1.0));
    CHECK(ring.radii.at(binAt(180)) == Catch::Approx(3.0));
}

TEST_CASE("cwPassageTubeBuilder borrows the neighbor ring for all-zero LRUD", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(1, 2, 3, 4)},
        {QStringLiteral("b"), QVector3D(4, 0, 0), lrud(0, 0, 0, 0)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const Builder::Ring& measured = ringAt(chain, QVector3D(0, 0, 0));
    const Builder::Ring& zeros = ringAt(chain, QVector3D(4, 0, 0));
    CHECK_FALSE(hasMeasuredBin(zeros));
    CHECK(zeros.radii == measured.radii);
    for (double radius : zeros.radii) {
        CHECK(radius >= Builder::kMinRadiusMeters);
    }
    CHECK(debug.warnings.isEmpty());
}

TEST_CASE("cwPassageTubeBuilder drops an out-of-range LRUD value with a warning", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(999, 1, 1, 1)},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.legs.size() == 1);
    CHECK_FALSE(debug.legs.at(0).fromLrud.left.has_value());
    REQUIRE(debug.legs.at(0).fromLrud.right.has_value());

    REQUIRE(debug.warnings.size() == 1);
    CHECK(debug.warnings.at(0).type == cwPassageTubeMesh::Warning::LrudOutOfRange);
    CHECK(debug.warnings.at(0).station == QStringLiteral("a"));
    CHECK(debug.warnings.at(0).tripId == input.region.caves.at(0).trips.at(0).id);

    // The left half is filled between up and down rather than measured.
    const Builder::Ring& ring = ringAt(debug.chains.at(0), QVector3D(0, 0, 0));
    CHECK(ring.measured.at(binAt(0)));
    CHECK_FALSE(ring.measured.at(binAt(180)));
    CHECK(ring.radii.at(binAt(180)) == Catch::Approx(1.0));

    const cwPassageTubeMesh mesh = Builder::build(input.region, input.positionsByCave, {}, Builder::Options());
    CHECK(mesh.warnings.size() == 1);
}

TEST_CASE("cwPassageTubeBuilder borrows the nearest ring for a station without LRUD", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(1, 1, 1, 1)},
        {QStringLiteral("b"), QVector3D(4, 0, 0)},
        {QStringLiteral("c"), QVector3D(14, 0, 0), lrud(2, 2, 2, 2)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const Builder::Ring& b = ringAt(chain, QVector3D(4, 0, 0));
    CHECK_FALSE(hasMeasuredBin(b));
    for (double radius : b.radii) {
        CHECK(radius == Catch::Approx(1.0));
    }

    const Builder::Ring& nearC = ringAt(chain, QVector3D(13, 0, 0));
    CHECK_FALSE(hasMeasuredBin(nearC));
    for (double radius : nearC.radii) {
        CHECK(radius == Catch::Approx(2.0));
    }
    CHECK(debug.warnings.isEmpty());
}

TEST_CASE("cwPassageTubeBuilder marks a borrowed ring unmeasured when its own LRUD fills too few bins", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), {QStringLiteral("1"), QString(), QString(), QString()}},
        {QStringLiteral("b"), QVector3D(10, 0, 0), lrud(2, 2, 2, 2)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const Builder::Ring& leftOnly = ringAt(chain, QVector3D(0, 0, 0));
    CHECK_FALSE(hasMeasuredBin(leftOnly));
    for (double radius : leftOnly.radii) {
        CHECK(radius == Catch::Approx(2.0));
    }
}

TEST_CASE("cwPassageTubeBuilder uses the default radius for a chain without passage data", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(10, 0, 0)},
    }}});

    Builder::Options options;
    options.ringResolution = 8;
    const Builder::Debug debug = Builder::buildDebug(input.region, input.positionsByCave, {}, options);
    REQUIRE(debug.chains.size() == 1);
    for (const Builder::Ring& ring : debug.chains.at(0).rings) {
        CHECK(ring.radii == QVector<double>(8, options.defaultRadiusMeters));
        CHECK_FALSE(hasMeasuredBin(ring));
    }

    REQUIRE(debug.warnings.size() == 1);
    CHECK(debug.warnings.at(0).type == cwPassageTubeMesh::Warning::NoPassageData);
    CHECK(debug.warnings.at(0).station == QStringLiteral("a"));
}

TEST_CASE("cwPassageTubeBuilder places pit dead-end caps by up and down", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("top"), QVector3D(0, 0, 0), lrud(1, 1, 2, 5)},
        {QStringLiteral("bottom"), QVector3D(0, 0, -10), lrud(1, 1, 4, 3)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    REQUIRE(chain.rings.size() == 11);

    // The chain starts climbing out of the top and ends descending past the bottom.
    CHECK(chain.startCapOffsetMeters == Catch::Approx(2.0));
    CHECK(chain.endCapOffsetMeters == Catch::Approx(3.0));

    // Pit rings get no LRUD samples and fall back to the default radius.
    CHECK_FALSE(hasMeasuredBin(chain.rings.at(5)));
}

TEST_CASE("cwPassageTubeBuilder skins a straight leg into a closed tube", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(1, 0, 0), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    const int resolution = Builder::Options().ringResolution;
    REQUIRE(debug.chains.size() == 1);
    REQUIRE(debug.chains.at(0).rings.size() == 2);
    CHECK(debug.chains.at(0).startCap == Builder::EndCap::Fan);
    CHECK(debug.chains.at(0).endCap == Builder::EndCap::Fan);

    const cwPassageTubeMesh& mesh = debug.mesh;
    // Two rings of vertices plus one fan center per end.
    CHECK(mesh.positions.size() == 2 * resolution + 2);
    // Two side triangles per bin plus one fan triangle per bin at each end.
    CHECK(mesh.indices.size() == 3 * (2 * resolution + 2 * resolution));
    CHECK(isClosedSurface(mesh.indices));
    CHECK(signedVolume(mesh) > 0.0);

    REQUIRE(mesh.tripIndexRanges.size() == 1);
    CHECK(mesh.tripIndexRanges.at(0).first == 0);
    CHECK(mesh.tripIndexRanges.at(0).count == mesh.indices.size());
    CHECK(mesh.tripUuids == QVector<QUuid>({input.region.caves.at(0).trips.at(0).id}));

    const cwPassageTubeMesh built = Builder::build(input.region, input.positionsByCave, {}, Builder::Options());
    CHECK(built.positions == mesh.positions);
    CHECK(built.indices == mesh.indices);
}

TEST_CASE("cwPassageTubeBuilder keeps a 90 degree bend wound outward without folds", "[PassageTube][cwPassageTubeBuilder]")
{
    const double radius = GENERATE(0.2, 3.0);
    const QStringList dimensions = lrud(radius, radius, radius, radius);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
        {QStringLiteral("c"), QVector3D(10, 10, 0), dimensions},
    }}});

    INFO("radius " << radius);
    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    checkNoFoldBack(debug.chains.at(0));
    CHECK(isClosedSurface(debug.mesh.indices));
    CHECK(signedVolume(debug.mesh) > 0.0);
}

TEST_CASE("cwPassageTubeBuilder rounds a 120 degree bend with inset rings", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const QVector3D corner(10, 0, 0);
    const QVector3D outgoing(-0.5f, std::sqrt(3.0f) / 2.0f, 0.0f);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), corner, dimensions},
        {QStringLiteral("c"), corner + outgoing * 10.0f, dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const Builder::Ring& before = ringAt(chain, corner - QVector3D(0.5f, 0, 0));
    checkVectorNear(before.frame.tangent, QVector3D(1, 0, 0));
    const Builder::Ring& after = ringAt(chain, corner + outgoing * 0.5f);
    checkVectorNear(after.frame.tangent, outgoing);
    checkFramesContinuous(chain);
    checkNoFoldBack(chain);

    CHECK(isClosedSurface(debug.mesh.indices));
    CHECK(signedVolume(debug.mesh) > 0.0);
}

TEST_CASE("cwPassageTubeBuilder leaves a gentle bend without inset rings", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(2, 0, 0)},
        {QStringLiteral("c"), QVector3D(4, 1, 0)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    // The three stations and one mid-shot sample on each leg.
    CHECK(debug.chains.at(0).rings.size() == 5);
}

TEST_CASE("cwPassageTubeBuilder pushes a junction arm into the through tube and domes it", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), dimensions};
    const StationInput d {QStringLiteral("d"), QVector3D(10, 10, 0), dimensions};
    const BuilderInput input = makeInput({{{a, b}, {b, d}, {b, c}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 2);

    const Builder::Chain& through = debug.chains.at(0);
    CHECK(through.startCap == Builder::EndCap::Fan);
    CHECK(through.endCap == Builder::EndCap::Fan);

    // The arm walks d → b, south into the through tube.
    const Builder::Chain& arm = debug.chains.at(1);
    CHECK(arm.startCap == Builder::EndCap::Fan);
    REQUIRE(arm.endCap == Builder::EndCap::Dome);
    checkVectorNear(arm.rings.last().center, QVector3D(10, -0.5, 0));

    // The dome's apex sits the ring's smallest radius past the pushed ring.
    CHECK(hasPosition(debug.mesh, QVector3D(10, -1.5, 0)));
    CHECK(isClosedSurface(debug.mesh.indices));
    CHECK(signedVolume(debug.mesh) > 0.0);
}

TEST_CASE("cwPassageTubeBuilder caps each trip of a chain that spans two trips", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), dimensions};
    const SurveyInput first {{{a, b}}};
    const SurveyInput second {{{b, c}}};
    const BuilderInput input = makeTripsInput({first, second});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);

    const cwPassageTubeMesh& mesh = debug.mesh;
    const QList<cwTripData>& trips = input.region.caves.at(0).trips;
    CHECK(mesh.tripUuids == QVector<QUuid>({trips.at(0).id, trips.at(1).id}));
    REQUIRE(mesh.tripIndexRanges.size() == 2);

    int next = 0;
    for (const cwPassageTubeMesh::VertexRange& range : mesh.tripIndexRanges) {
        CHECK(range.first == next);
        CHECK(range.count > 0);
        CHECK(isClosedSurface(mesh.indices, range.first, range.count));
        next = range.first + range.count;
    }
    CHECK(next == mesh.indices.size());
    CHECK(signedVolume(mesh) > 0.0);
}

TEST_CASE("cwPassageTubeBuilder places two rings on a shot shorter than the spacing", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(0.3f, 0, 0)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    REQUIRE(debug.chains.at(0).rings.size() == 2);
    CHECK(debug.chains.at(0).rings.at(0).center.distanceToPoint(debug.chains.at(0).rings.at(1).center) == Catch::Approx(0.3));
    CHECK(isClosedSurface(debug.mesh.indices));
}

TEST_CASE("cwPassageTubeBuilder merges rings closer than the minimum spacing", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(2, 0, 0)},
        {QStringLiteral("c"), QVector3D(2.03f, 0, 0)},
        {QStringLiteral("d"), QVector3D(4, 0, 0)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    for (int i = 1; i < chain.rings.size(); ++i) {
        INFO("ring " << i);
        CHECK(chain.rings.at(i).center.distanceToPoint(chain.rings.at(i - 1).center) >= 0.05f);
    }
    CHECK(isClosedSurface(debug.mesh.indices));
}

TEST_CASE("cwPassageTubeBuilder closes a loop without caps", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(10, 0, 0)},
        {QStringLiteral("c"), QVector3D(5, 8, 0)},
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).startCap == Builder::EndCap::None);
    CHECK(debug.chains.at(0).endCap == Builder::EndCap::None);
    CHECK(isClosedSurface(debug.mesh.indices));
}

TEST_CASE("cwPassageTubeBuilder keeps a loop closing at a sharp bend without folds", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(3, 3, 3, 3);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
        {QStringLiteral("c"), QVector3D(5, 8, 0), dimensions},
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    REQUIRE(debug.chains.at(0).startCap == Builder::EndCap::None);
    checkNoFoldBack(debug.chains.at(0));
    CHECK(isClosedSurface(debug.mesh.indices));
}
