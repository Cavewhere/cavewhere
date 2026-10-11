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
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QElapsedTimer>
#include <QStringList>

//Std includes
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using Builder = cwPassageTubeBuilder;

using StationInput = cwPassageTubeCases::Station;
using SurveyInput = cwPassageTubeCases::Trip;
using BuilderInput = cwPassageTubeCases::Survey;

BuilderInput makeInput(const SurveyInput& survey)
{
    return cwPassageTubeCases::toSurvey(survey);
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

void checkFramesOrthonormal(const Builder::Chain& chain)
{
    for (int i = 0; i < chain.rings.size(); ++i) {
        const Builder::Frame& frame = chain.rings.at(i).frame;
        INFO("ring " << i);
        REQUIRE(isFinite(frame.up));
        REQUIRE(isFinite(frame.right));
        CHECK(frame.up.length() == Catch::Approx(1.0).margin(1e-4));
        CHECK(std::abs(QVector3D::dotProduct(frame.up, frame.tangent)) < 1e-4f);
    }
}

// Each trip's tube is closed, consistently wound and outward facing.
void checkMeshClosed(const cwPassageTubeMesh& mesh)
{
    for (const cwPassageTubeMesh::VertexRange& range : mesh.tripIndexRanges) {
        const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(mesh, range);
        INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
        CHECK(report.issues.isEmpty());
    }
}

// The tube is closed and its rings neither twist, fold back nor jump.
void checkMeshClosedWithSaneRings(const Builder::Debug& debug)
{
    checkMeshClosed(debug.mesh);
    const QVector<cwPassageTubeMeshCheck::Issue> ringIssues = cwPassageTubeMeshCheck::ringIssues(debug);
    INFO(cwPassageTubeMeshCheck::toText({ringIssues, 0, {}}).toStdString());
    CHECK(ringIssues.isEmpty());
}

// Every artifact check passes on the built mesh and rings.
void checkMeshClean(const Builder::Debug& debug)
{
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
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

// The stations of the ShaftReading warnings, sorted.
QStringList shaftWarningStations(const Builder::Debug& debug)
{
    QStringList stations;
    for (const cwPassageTubeMesh::Warning& warning : debug.warnings) {
        if (warning.type == cwPassageTubeMesh::Warning::ShaftReading) {
            stations.append(warning.station);
        }
    }
    stations.sort();
    return stations;
}

bool hasPosition(const cwPassageTubeMesh& mesh, const QVector3D& position)
{
    return std::any_of(mesh.positions.cbegin(), mesh.positions.cend(), [&](const QVector3D& p) {
        return p.distanceToPoint(position) < 1e-4f;
    });
}

void checkFarthestVertexNear(const Builder::Ring& ring, const QVector3D& expected, float tolerance)
{
    const QVector3D farthest = *std::max_element(ring.vertices.cbegin(), ring.vertices.cend(), [&](const QVector3D& x, const QVector3D& y) {
        return x.distanceToPoint(ring.center) < y.distanceToPoint(ring.center);
    });
    INFO("farthest (" << farthest.x() << ", " << farthest.y() << ", " << farthest.z() << ")");
    CHECK(farthest.distanceToPoint(expected) < tolerance);
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
    checkFramesOrthonormal(chain);
    checkMeshClean(debug);

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

    // Between the axes the ring follows the ellipse of the two adjacent
    // dimensions: its polar radius 2·3 / hypot(3·cos45°, 2·sin45°).
    CHECK(chain.rings.at(0).radii.at(binAt(45)) == Catch::Approx(2.3534).margin(1e-4));
    CHECK(debug.warnings.isEmpty());

    // Level dead ends sit flush with their stations.
    CHECK(chain.startCapOffsetMeters == Catch::Approx(0.0));
    CHECK(chain.endCapOffsetMeters == Catch::Approx(0.0));
}

TEST_CASE("cwPassageTubeBuilder fits a circle to equal LRUD", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1.5, 1.5, 1.5, 1.5);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    for (const Builder::Ring& ring : debug.chains.at(0).rings) {
        for (double radius : ring.radii) {
            CHECK(radius == Catch::Approx(1.5));
        }
    }
}

namespace {

BuilderInput verticalPitInput()
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(0, 10, 0), dimensions},
        {QStringLiteral("c"), QVector3D(0, 10, -10), dimensions},
        {QStringLiteral("d"), QVector3D(0, 20, -10), dimensions},
    }}});
}

BuilderInput pitOnNewBearingInput()
{
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(0, 10, 0)},
        {QStringLiteral("c"), QVector3D(0, 10, -10)},
        {QStringLiteral("d"), QVector3D(10, 10, -10)},
    }}});
}

BuilderInput rightAngleBendInput(double radius)
{
    const QStringList dimensions = lrud(radius, radius, radius, radius);
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
        {QStringLiteral("c"), QVector3D(10, 10, 0), dimensions},
    }}});
}

const QVector3D kBendCorner(10, 0, 0);
const QVector3D kBend120Outgoing(-0.5f, std::sqrt(3.0f) / 2.0f, 0.0f);

BuilderInput bend120Input()
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), kBendCorner, dimensions},
        {QStringLiteral("c"), kBendCorner + kBend120Outgoing * 10.0f, dimensions},
    }}});
}

BuilderInput triangleLoopInput(const QStringList& dimensions = {QString(), QString(), QString(), QString()})
{
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
        {QStringLiteral("c"), QVector3D(5, 8, 0), dimensions},
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
    }}});
}

} // namespace

TEST_CASE("cwPassageTubeBuilder keeps frames continuous through a vertical pit", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(verticalPitInput());
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    checkFramesOrthonormal(chain);
    checkMeshClosedWithSaneRings(debug);

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
    const Builder::Debug debug = buildDebug(pitOnNewBearingInput());
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    checkFramesOrthonormal(chain);
    checkMeshClosedWithSaneRings(debug);

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
    checkFramesOrthonormal(chain);
    checkMeshClean(debug);
    checkVectorNear(chain.rings.first().frame.up, QVector3D(0, 1, 0));
}

TEST_CASE("cwPassageTubeBuilder keeps a ring's world shape whichever way the chunk is entered", "[PassageTube][cwPassageTubeBuilder]")
{
    // The same passage surveyed both ways; stored LRUD follow the chunk's
    // station order, so left and right trade places. The backward survey
    // splits into b→a and c→b, so its chain walks a→b→c against both
    // chunks while b's record faces west.
    const QStringList ends = lrud(1, 1, 1, 1);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), ends};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), ends};
    const QVector3D bPosition(10, 0, 0);
    const StationInput forwardB {QStringLiteral("b"), bPosition, lrud(5, 1, 1, 1)};
    const StationInput backwardB {QStringLiteral("b"), bPosition, lrud(1, 5, 1, 1)};
    const StationInput bWithoutLrud {QStringLiteral("b"), bPosition};

    const Builder::Debug forward = buildDebug(makeInput({{{a, forwardB, c}}}));
    const Builder::Debug backward = buildDebug(makeInput({{{backwardB, a}, {c, bWithoutLrud}}}));
    REQUIRE(forward.chains.size() == 1);
    REQUIRE(backward.chains.size() == 1);
    const QStringList walk {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    CHECK(chainStations(forward, forward.chains.at(0)) == walk);
    CHECK(chainStations(backward, backward.chains.at(0)) == walk);

    const Builder::Ring& forwardRing = ringAt(forward.chains.at(0), bPosition);
    const Builder::Ring& backwardRing = ringAt(backward.chains.at(0), bPosition);
    REQUIRE(forwardRing.vertices.size() == backwardRing.vertices.size());
    for (const QVector3D& vertex : forwardRing.vertices) {
        INFO("vertex (" << vertex.x() << ", " << vertex.y() << ", " << vertex.z() << ")");
        CHECK(std::any_of(backwardRing.vertices.cbegin(), backwardRing.vertices.cend(), [&](const QVector3D& other) {
            return other.distanceToPoint(vertex) < 1e-3f;
        }));
    }
}

TEST_CASE("cwPassageTubeBuilder takes the component of a wall across the ring", "[PassageTube][cwPassageTubeBuilder]")
{
    // b faces the chord a→c, (1, 1, 0)/√2, so its 5 m left wall lies toward
    // (−1, 1, 0)/√2 and each ring through b sees 5·cos45° of it. The chunk
    // b→d comes first so the chain rule runs the through chain d–b–c.
    constexpr float kComponent = 5.0f * std::numbers::sqrt2_v<float> / 2.0f;
    constexpr float kToleranceMeters = 0.2f;
    const QStringList ends = lrud(1, 1, 1, 1);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), ends};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), lrud(5, 0, 1, 1)};
    const StationInput c {QStringLiteral("c"), QVector3D(10, 10, 0), ends};
    const StationInput d {QStringLiteral("d"), QVector3D(10, -10, 0)};
    const StationInput bWithoutLrud {QStringLiteral("b"), QVector3D(10, 0, 0)};
    const BuilderInput input = makeInput({{{bWithoutLrud, d}, {a, b, c}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 2);
    const auto passesThroughB = [&](const Builder::Chain& chain) { return chain.legs.size() == 2; };
    const auto through = std::find_if(debug.chains.cbegin(), debug.chains.cend(), passesThroughB);
    const auto arm = std::find_if_not(debug.chains.cbegin(), debug.chains.cend(), passesThroughB);
    REQUIRE(through != debug.chains.cend());
    REQUIRE(arm != debug.chains.cend());
    CHECK(chainStations(debug, *through) == QStringList({QStringLiteral("d"), QStringLiteral("b"), QStringLiteral("c")}));
    CHECK(chainStations(debug, *arm) == QStringList({QStringLiteral("a"), QStringLiteral("b")}));

    const QVector3D bPosition = b.position.value();
    checkFarthestVertexNear(ringAt(*through, bPosition), bPosition + QVector3D(-kComponent, 0, 0), kToleranceMeters);

    const Builder::Ring& armEnd = arm->rings.last();
    checkVectorNear(armEnd.center, bPosition);
    checkFarthestVertexNear(armEnd, bPosition + QVector3D(0, kComponent, 0), kToleranceMeters);
}

TEST_CASE("cwPassageTubeBuilder falls back to the next station's facing on a vertical shot", "[PassageTube][cwPassageTubeBuilder]")
{
    // b's chord in chunk 2 drops straight down, so b takes q's facing (the
    // chord b→r, west) and its 3 m left wall lies to the south.
    constexpr float kToleranceMeters = 0.2f;
    const QVector3D bPosition(10, 0, 0);
    const BuilderInput input = makeInput({{
        {
            {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(1, 1, 1, 1)},
            {QStringLiteral("b"), bPosition},
        },
        {
            {QStringLiteral("b"), bPosition, lrud(3, 0, 1, 1)},
            {QStringLiteral("q"), QVector3D(10, 0, -10)},
            {QStringLiteral("r"), QVector3D(0, 0, -10)},
        },
    }});

    const Builder::Debug debug = buildDebug(input);
    const auto drop = std::find_if(debug.legs.cbegin(), debug.legs.cend(), [](const Builder::Leg& leg) {
        return leg.from == QStringLiteral("b") && leg.to == QStringLiteral("q");
    });
    REQUIRE(drop != debug.legs.cend());
    checkVectorNear(drop->fromFacing, QVector3D(-1, 0, 0));
    checkVectorNear(drop->toFacing, QVector3D(-1, 0, 0));

    REQUIRE(debug.chains.size() == 1);
    checkFarthestVertexNear(ringAt(debug.chains.at(0), bPosition), bPosition + QVector3D(0, -3, 0), kToleranceMeters);
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

TEST_CASE("cwPassageTubeBuilder keeps the pit depth out of the lip ring and rounds the shaft", "[PassageTube][cwPassageTubeBuilder]")
{
    // The lip's down and the bottom's up measure the 20 m shaft, which the
    // b→c leg already surveys.
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(2, 2, 1, 0.5)},
        {QStringLiteral("b"), QVector3D(0, 10, 0), lrud(2, 2, 1, 20)},
        {QStringLiteral("c"), QVector3D(0, 10, -20), lrud(2, 2, 20, 0.5)},
        {QStringLiteral("d"), QVector3D(0, 20, -20), lrud(2, 2, 1, 0.5)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    // The approach floor continues to the lip in place of the dropped reading.
    const Builder::Ring& lip = ringAt(chain, QVector3D(0, 10, 0));
    CHECK(lip.radii.at(binAt(90)) == Catch::Approx(1.0));
    CHECK(lip.radii.at(binAt(270)) == Catch::Approx(0.5));
    const Builder::Ring& approach = ringAt(chain, QVector3D(0, 9, 0));
    CHECK(approach.radii.at(binAt(270)) == Catch::Approx(0.5));

    const Builder::Ring& bottom = ringAt(chain, QVector3D(0, 10, -20));
    CHECK(bottom.radii.at(binAt(90)) == Catch::Approx(1.0));
    CHECK(bottom.radii.at(binAt(270)) == Catch::Approx(0.5));

    // The shaft is round at the lip's horizontal half-width.
    const Builder::Ring& shaft = ringAt(chain, QVector3D(0, 10, -10));
    CHECK_FALSE(hasMeasuredBin(shaft));
    for (double radius : shaft.radii) {
        CHECK(radius == Catch::Approx(2.0));
    }
    for (const Builder::Ring& ring : chain.rings) {
        for (double radius : ring.radii) {
            CHECK(radius <= 2.0 + 1e-6);
        }
    }
    CHECK(shaftWarningStations(debug) == QStringList {QStringLiteral("b"), QStringLiteral("c")});
}

TEST_CASE("cwPassageTubeBuilder drops up and down that run along a steep leg", "[PassageTube][cwPassageTubeBuilder]")
{
    // A 60° ramp: the top's down and the bottom's up point along the leg and
    // are left out; the ramp is too steep to continue the other end's reading.
    const BuilderInput input = makeInput({{{
        {QStringLiteral("top"), QVector3D(0, 0, 0), lrud(1, 1, 1, 9)},
        {QStringLiteral("bottom"), QVector3D(0, 10, -17.3205), lrud(1, 1, 9, 1)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const Builder::Ring& top = ringAt(chain, QVector3D(0, 0, 0));
    CHECK(top.measured.at(binAt(90)));
    CHECK(top.radii.at(binAt(90)) == Catch::Approx(1.0));
    CHECK_FALSE(top.measured.at(binAt(270)));
    CHECK(top.radii.at(binAt(270)) == Catch::Approx(1.0));

    const Builder::Ring& bottom = ringAt(chain, QVector3D(0, 10, -17.3205));
    CHECK(bottom.measured.at(binAt(270)));
    CHECK_FALSE(bottom.measured.at(binAt(90)));
    CHECK(bottom.radii.at(binAt(90)) == Catch::Approx(1.0));

    // The caps still use the raw outward readings, scaled by the ramp's weight.
    CHECK(chain.startCapOffsetMeters == Catch::Approx(0.5).margin(1e-4));
    CHECK(chain.endCapOffsetMeters == Catch::Approx(0.5).margin(1e-4));
}

TEST_CASE("cwPassageTubeBuilder keeps a side pit's depth out of the through passage ring", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{
        {
            {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(1, 1, 1, 1)},
            {QStringLiteral("b"), QVector3D(0, 10, 0), lrud(1, 1, 1, 20)},
            {QStringLiteral("c"), QVector3D(0, 20, 0), lrud(1, 1, 1, 1)},
        },
        {
            {QStringLiteral("b"), QVector3D(0, 10, 0), lrud(1, 1, 1, 20)},
            {QStringLiteral("p"), QVector3D(0, 10, -20), lrud(1, 1, 20, 1)},
        },
    }});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 2);
    const auto isThrough = [](const Builder::Chain& chain) { return chain.rings.first().center == QVector3D(0, 0, 0); };
    const auto through = std::find_if(debug.chains.cbegin(), debug.chains.cend(), isThrough);
    REQUIRE(through != debug.chains.cend());
    for (const Builder::Ring& ring : through->rings) {
        for (double radius : ring.radii) {
            CHECK(radius == Catch::Approx(1.0));
        }
    }
    const auto arm = std::find_if_not(debug.chains.cbegin(), debug.chains.cend(), isThrough);
    REQUIRE(arm != debug.chains.cend());
    for (const Builder::Ring& ring : arm->rings) {
        for (double radius : ring.radii) {
            CHECK(radius <= 1.0 + 1e-6);
        }
    }
}

TEST_CASE("cwPassageTubeBuilder keeps a dead end's pit depth out of the passage floor", "[PassageTube][cwPassageTubeBuilder]")
{
    // b's 35 m down is the depth of an unsurveyed pit at the dead end.
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(1, 0.2, 3, 0.5)},
        {QStringLiteral("b"), QVector3D(0, 10, 0), lrud(0, 2, 8, 35)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    for (int i = 0; i < chain.rings.size(); ++i) {
        INFO("ring " << i);
        CHECK(chain.rings.at(i).radii.at(binAt(270)) == Catch::Approx(0.5));
    }
    CHECK(ringAt(chain, QVector3D(0, 10, 0)).radii.at(binAt(90)) == Catch::Approx(8.0));

    REQUIRE(debug.warnings.size() == 1);
    const cwPassageTubeMesh::Warning& warning = debug.warnings.first();
    CHECK(warning.type == cwPassageTubeMesh::Warning::ShaftReading);
    CHECK(warning.station == QStringLiteral("b"));
    CHECK(warning.tripId == input.region.caves.first().trips.first().id);
    CHECK(debug.mesh.warnings.size() == 1);
}

TEST_CASE("cwPassageTubeBuilder keeps a tall canyon's ceiling", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(0.5, 0.5, 10, 0.5);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(0, 10, 0), dimensions},
        {QStringLiteral("c"), QVector3D(0, 20, 0), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    for (const Builder::Ring& ring : debug.chains.at(0).rings) {
        CHECK(ring.radii.at(binAt(90)) == Catch::Approx(10.0));
    }
    CHECK(debug.warnings.isEmpty());
}

TEST_CASE("cwPassageTubeBuilder keeps a shaft depth repeated on both stations of a leg out of its rings", "[PassageTube][cwPassageTubeBuilder]")
{
    // As at Phake Cave's a4 and a6: both ends of x→y carry the 23 m depth of
    // the pit that y→q drops down.
    constexpr double kWidth = 5.0 + Builder::kMinRadiusMeters;
    const BuilderInput input = makeInput({{
        {
            {QStringLiteral("w"), QVector3D(0, -10, 0), lrud(1, 1, 2, 1)},
            {QStringLiteral("x"), QVector3D(0, 0, 0), lrud(1, 1, 2, 1.7)},
            {QStringLiteral("v"), QVector3D(10, 0, 0), lrud(1, 1, 2, 2)},
        },
        {
            {QStringLiteral("x"), QVector3D(0, 0, 0), lrud(5, 0, 3, 23)},
            {QStringLiteral("y"), QVector3D(0, 15, 0), lrud(5, 0, 3, 23)},
            {QStringLiteral("z"), QVector3D(0, 25, 0), lrud(1, 1, 2, 1)},
        },
        {
            {QStringLiteral("y"), QVector3D(0, 15, 0), lrud(1, 5, 2, 1.2)},
            {QStringLiteral("q"), QVector3D(0, 15, -23), lrud(1, 1, 1, 1)},
        },
    }});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE_FALSE(debug.chains.isEmpty());
    for (int c = 0; c < debug.chains.size(); ++c) {
        const Builder::Chain& chain = debug.chains.at(c);
        for (int i = 0; i < chain.rings.size(); ++i) {
            INFO("chain " << c << " ring " << i);
            for (double radius : chain.rings.at(i).radii) {
                CHECK(radius <= kWidth + 1e-6);
            }
        }
    }

    CHECK(shaftWarningStations(debug) == QStringList {QStringLiteral("x"), QStringLiteral("y")});
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
    checkMeshClean(debug);

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
    INFO("radius " << radius);
    const Builder::Debug debug = buildDebug(rightAngleBendInput(radius));
    REQUIRE(debug.chains.size() == 1);
    checkMeshClosedWithSaneRings(debug);
}

TEST_CASE("cwPassageTubeBuilder keeps a 90 degree bend narrower than its corner clean", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kNarrowRadiusMeters = 0.2;
    checkMeshClean(buildDebug(rightAngleBendInput(kNarrowRadiusMeters)));
}

TEST_CASE("cwPassageTubeBuilder rounds a 120 degree bend with inset rings", "[PassageTube][cwPassageTubeBuilder]")
{
    const QVector3D& corner = kBendCorner;
    const QVector3D& outgoing = kBend120Outgoing;
    const Builder::Debug debug = buildDebug(bend120Input());
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const Builder::Ring& before = ringAt(chain, corner - QVector3D(0.5f, 0, 0));
    checkVectorNear(before.frame.tangent, QVector3D(1, 0, 0));
    const Builder::Ring& after = ringAt(chain, corner + outgoing * 0.5f);
    checkVectorNear(after.frame.tangent, outgoing);
    checkFramesOrthonormal(chain);
    checkMeshClosedWithSaneRings(debug);
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

namespace {

// The T of a through chunk a→b→c along +x and an arm chunk d→b from +y.
BuilderInput junctionInput(const QStringList& junctionLrud)
{
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), junctionLrud};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0)};
    const StationInput d {QStringLiteral("d"), QVector3D(10, 10, 0)};
    const StationInput armB {QStringLiteral("b"), QVector3D(10, 0, 0)};
    return makeInput({{{a, b, c}, {d, armB}}});
}

// The arm walks d → b, south into the through tube, and ends at b.
const Builder::Chain& junctionArm(const Builder::Debug& debug)
{
    REQUIRE(debug.chains.size() == 2);
    const Builder::Chain& arm = debug.chains.at(1);
    REQUIRE(chainStations(debug, arm) == QStringList({QStringLiteral("d"), QStringLiteral("b")}));
    CHECK(arm.startCap == Builder::EndCap::Fan);
    REQUIRE(arm.endCap == Builder::EndCap::Dome);
    checkVectorNear(arm.rings.last().center, QVector3D(10, 0, 0));
    return arm;
}

} // namespace

TEST_CASE("cwPassageTubeBuilder domes a junction arm to the through passage's far wall", "[PassageTube][cwPassageTubeBuilder][!mayfail]")
{
    const Builder::Debug debug = buildDebug(junctionInput(lrud(2, 2, 2, 2)));
    const Builder::Chain& arm = junctionArm(debug);

    // b faces +x, so its 2 m right wall lies to the south, along the arm.
    CHECK(arm.endDomeHeightMeters == Catch::Approx(2.0));
    CHECK(hasPosition(debug.mesh, QVector3D(10, -2, 0)));
    // The dome overlaps the through tube until the T07d stitch pass merges them.
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder takes the dome height from a junction without readings", "[PassageTube][cwPassageTubeBuilder][!mayfail]")
{
    const Builder::Debug debug = buildDebug(junctionInput({QString(), QString(), QString(), QString()}));
    const Builder::Chain& arm = junctionArm(debug);

    const double defaultRadius = Builder::Options().defaultRadiusMeters;
    CHECK(arm.endDomeHeightMeters == Catch::Approx(defaultRadius));
    CHECK(hasPosition(debug.mesh, QVector3D(10, -static_cast<float>(defaultRadius), 0)));
    // The dome overlaps the through tube until the T07d stitch pass merges them.
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder caps each trip of a chain that spans two trips", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), dimensions};
    const SurveyInput first {{{a, b}}};
    const SurveyInput second {{{b, c}}};
    const BuilderInput input = cwPassageTubeCases::toSurvey(QList<SurveyInput> {first, second});

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
        next = range.first + range.count;
    }
    CHECK(next == mesh.indices.size());
    checkMeshClean(debug);
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
    checkMeshClean(debug);
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
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder closes a loop without caps", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(triangleLoopInput());
    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).startCap == Builder::EndCap::None);
    CHECK(debug.chains.at(0).endCap == Builder::EndCap::None);
    checkMeshClosedWithSaneRings(debug);
}

TEST_CASE("cwPassageTubeBuilder keeps a loop closing at a sharp bend without folds", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(triangleLoopInput(lrud(3, 3, 3, 3)));
    REQUIRE(debug.chains.size() == 1);
    REQUIRE(debug.chains.at(0).startCap == Builder::EndCap::None);
    checkMeshClosedWithSaneRings(debug);
}

// The inner wall of a bend sharper than the tube's radius still crosses
// itself: SelfIntersection, DegenerateTriangle and Crease fire where pulled
// vertices land on the previous ring, and WallMissed where the inner wall
// collapses. No plan task handles it yet; this records the failures until
// one does.
TEST_CASE("cwPassageTubeBuilder keeps the inner wall of a sharp bend clean", "[PassageTube][cwPassageTubeBuilder][!mayfail]")
{
    const QList<std::pair<QString, BuilderInput>> bends = {
        {QStringLiteral("vertical pit"), verticalPitInput()},
        {QStringLiteral("pit on a new bearing"), pitOnNewBearingInput()},
        {QStringLiteral("90 degree bend, radius 3"), rightAngleBendInput(3.0)},
        {QStringLiteral("120 degree bend"), bend120Input()},
        {QStringLiteral("loop without caps"), triangleLoopInput()},
        {QStringLiteral("loop closing at a sharp bend"), triangleLoopInput(lrud(3, 3, 3, 3))},
    };
    for (const auto& [name, input] : bends) {
        INFO(name.toStdString());
        checkMeshClean(buildDebug(input));
    }
}

TEST_CASE("cwPassageTubeBuilder keeps the Phake Cave a11 to a12 leg a single sane tube", "[PassageTube][cwPassageTubeBuilder]")
{
    // The largest wall readings on the leg: a11's left, a12's right and
    // ceiling. a12's 35 m down is the depth of an unsurveyed pit, not a wall.
    constexpr double kMaxLeftMeters = 1.0;
    constexpr double kMaxRightMeters = 2.0;
    constexpr double kMaxRadiusMeters = 8.0;
    constexpr double kToleranceMeters = 1e-3;
    constexpr qint64 kMaxCheckMilliseconds = 200;

    const BuilderInput input = makeInput(cwPassageTubeCases::phakeCave());
    const Builder::Debug debug = buildDebug(input);

    const auto legIt = std::find_if(debug.legs.cbegin(), debug.legs.cend(), [](const Builder::Leg& leg) {
        return leg.from == QStringLiteral("a11") && leg.to == QStringLiteral("a12");
    });
    REQUIRE(legIt != debug.legs.cend());
    const int legIndex = static_cast<int>(legIt - debug.legs.cbegin());
    const QVector3D a11 = legIt->fromPosition;
    const QVector3D a12 = legIt->toPosition;

    QVector<int> chainsWithLeg;
    for (int c = 0; c < debug.chains.size(); ++c) {
        const QVector<Builder::ChainLeg>& legs = debug.chains.at(c).legs;
        if (std::any_of(legs.cbegin(), legs.cend(), [&](const Builder::ChainLeg& chainLeg) { return chainLeg.legIndex == legIndex; })) {
            chainsWithLeg.append(c);
        }
    }
    REQUIRE(chainsWithLeg.size() == 1);
    const Builder::Chain& chain = debug.chains.at(chainsWithLeg.first());

    const auto ringIndexAt = [&](const QVector3D& center) {
        const auto it = std::find_if(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& ring) {
            return ring.center.distanceToPoint(center) < kToleranceMeters;
        });
        REQUIRE(it != chain.rings.cend());
        return static_cast<int>(it - chain.rings.cbegin());
    };
    const int a11Ring = ringIndexAt(a11);
    const int a12Ring = ringIndexAt(a12);
    const int step = a11Ring <= a12Ring ? 1 : -1;

    const QVector3D legVector = a12 - a11;
    const float legLength = legVector.length();
    const QVector3D legDirection = legVector / legLength;
    float previousAlong = -kToleranceMeters;
    for (int i = a11Ring; i != a12Ring + step; i += step) {
        const Builder::Ring& ring = chain.rings.at(i);
        INFO("ring " << i << " at (" << ring.center.x() << ", " << ring.center.y() << ", " << ring.center.z() << ")");

        // The rings run from a11 to a12 in order, on the shot.
        const float along = QVector3D::dotProduct(ring.center - a11, legDirection);
        CHECK(along > previousAlong);
        CHECK(along <= legLength + kToleranceMeters);
        CHECK(ring.center.distanceToPoint(a11 + legDirection * along) < kToleranceMeters);
        previousAlong = along;

        for (int bin = 0; bin < ring.radii.size(); ++bin) {
            INFO("bin " << bin);
            CHECK(ring.radii.at(bin) <= kMaxRadiusMeters + kToleranceMeters);
        }

        // One tube inside the measured walls: no vertex reaches past the
        // widest left or right wall or above the highest ceiling.
        for (int bin = 0; bin < ring.vertices.size(); ++bin) {
            INFO("vertex " << bin);
            const QVector3D offset = ring.vertices.at(bin) - ring.center;
            const float sideways = QVector3D::dotProduct(offset, ring.frame.right);
            CHECK(sideways <= kMaxRightMeters + kToleranceMeters);
            CHECK(-sideways <= kMaxLeftMeters + kToleranceMeters);
            CHECK(QVector3D::dotProduct(offset, ring.frame.up) <= kMaxRadiusMeters + kToleranceMeters);
        }
    }

    // Phake Cave has junctions; until T07d only its topology is clean.
    checkMeshClosed(debug.mesh);

    QElapsedTimer timer;
    timer.start();
    cwPassageTubeMeshCheck::check(debug.mesh, debug);
    CHECK(timer.elapsed() < kMaxCheckMilliseconds);
}

TEST_CASE("cwPassageTubeBuilder joins the Phake Cave arms to their through passages", "[PassageTube][cwPassageTubeBuilder][!mayfail]")
{
    constexpr float kToleranceMeters = 1e-3f;
    constexpr double kMinDomeHeightMeters = 1.5;
    constexpr double kMaxDomeHeightMeters = 2.5;

    const BuilderInput input = makeInput(cwPassageTubeCases::phakeCave());
    const Builder::Debug debug = buildDebug(input);

    // The chain holding the leg between armStation and junction, which must
    // end at junction in a dome; outward is the arm's direction there.
    struct Arm {
        const Builder::Chain* chain = nullptr;
        QVector3D junction;
        QVector3D outward;
        double domeHeight = 0.0;
    };
    const auto findArm = [&](const QString& armStation, const QString& junction) {
        Arm arm;
        for (const Builder::Chain& chain : debug.chains) {
            for (int i = 0; i < chain.legs.size(); ++i) {
                const Builder::ChainLeg& chainLeg = chain.legs.at(i);
                const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
                const bool joins = (leg.from == armStation && leg.to == junction)
                                   || (leg.from == junction && leg.to == armStation);
                if (!joins) {
                    continue;
                }
                const bool endsAtJunction = i == chain.legs.size() - 1 && chainEnd(debug, chainLeg) == junction;
                const bool startsAtJunction = i == 0 && chainStart(debug, chainLeg) == junction;
                REQUIRE((endsAtJunction || startsAtJunction));
                CHECK((endsAtJunction ? chain.endCap : chain.startCap) == Builder::EndCap::Dome);
                const bool fromIsJunction = leg.from == junction;
                arm.chain = &chain;
                arm.junction = fromIsJunction ? leg.fromPosition : leg.toPosition;
                arm.outward = (arm.junction - (fromIsJunction ? leg.toPosition : leg.fromPosition)).normalized();
                arm.domeHeight = endsAtJunction ? chain.endDomeHeightMeters : chain.startDomeHeightMeters;
            }
        }
        REQUIRE(arm.chain != nullptr);
        return arm;
    };

    const auto checkNoRingPastJunction = [&](const Arm& arm) {
        for (const Builder::Ring& ring : arm.chain->rings) {
            INFO("ring at (" << ring.center.x() << ", " << ring.center.y() << ", " << ring.center.z() << ")");
            CHECK(QVector3D::dotProduct(ring.center - arm.junction, arm.outward) <= kToleranceMeters);
        }
    };

    const Arm a6Arm = findArm(QStringLiteral("a4"), QStringLiteral("a6"));
    checkNoRingPastJunction(a6Arm);

    // a10's 2 m right wall, chunk 3 facing about SSW, projected on the arm.
    const Arm a10Arm = findArm(QStringLiteral("a13"), QStringLiteral("a10"));
    checkNoRingPastJunction(a10Arm);
    CHECK(a10Arm.domeHeight >= kMinDomeHeightMeters);
    CHECK(a10Arm.domeHeight <= kMaxDomeHeightMeters);
    CHECK(hasPosition(debug.mesh, a10Arm.junction + a10Arm.outward * static_cast<float>(a10Arm.domeHeight)));

    // The domes overlap their through tubes until the T07d stitch pass merges them.
    checkMeshClean(debug);
}
