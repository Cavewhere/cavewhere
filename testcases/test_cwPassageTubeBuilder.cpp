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
#include "cwPassageTubeGeometry.h"
#include "cwPassageTubeMeshCheck.h"
#include "cwPassageTubeUnion.h"

//Qt includes
#include <QElapsedTimer>
#include <QSet>
#include <QStringList>
#include <QVector2D>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <tuple>

namespace {

using Builder = cwPassageTubeBuilder;

using StationInput = cwPassageTubeCases::Station;
using SurveyInput = cwPassageTubeCases::Trip;
using BuilderInput = cwPassageTubeCases::Survey;

BuilderInput makeInput(const SurveyInput& survey)
{
    return cwPassageTubeCases::toSurvey(survey);
}

Builder::Debug buildDebug(const BuilderInput& input, const Builder::Options& options = Builder::Options())
{
    return Builder::buildDebug(input.region, input.positionsByCave, {}, options);
}

// Vertical rings on every leg, a plumb leg padded mean(L, R) / 2 off its
// stations: the builder before shafts.
Builder::Options verticalRingOptions()
{
    constexpr double kHalfWidthPadScale = 0.5;
    Builder::Options options;
    options.shaftRingsSquare = false;
    options.padScale = kHalfWidthPadScale;
    return options;
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

QVector3D binDirection(const Builder::Ring& ring, int bin)
{
    const double theta = ring.binAngles.at(bin);
    return ring.frame.right * static_cast<float>(std::cos(theta)) + ring.frame.up * static_cast<float>(std::sin(theta));
}

const Builder::Ring& ringAt(const Builder::Chain& chain, const QVector3D& center)
{
    const auto it = std::find_if(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& ring) {
        return ring.center.distanceToPoint(center) < 1e-4f;
    });
    REQUIRE(it != chain.rings.cend());
    return *it;
}

// The ring nearest a station: the station's section, which carries its
// readings.
const Builder::Ring& stationRing(const Builder::Chain& chain, const QVector3D& station)
{
    REQUIRE_FALSE(chain.rings.isEmpty());
    return *std::min_element(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& a, const Builder::Ring& b) {
        return a.center.distanceToPoint(station) < b.center.distanceToPoint(station);
    });
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

// The tube drawn with every trip visible is closed, consistently wound and
// outward facing.
void checkMeshClosed(const cwPassageTubeMesh& mesh)
{
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(mesh, cwPassageTubeMeshCheck::allTripsVisible(mesh));
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
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

// Every ring vertex sits at its bin's radius and offset: a loft keeps each ring
// in front of the one before, so the fold-back pull never moved a vertex.
void checkNoVertexPulled(const Builder::Debug& debug)
{
    for (const Builder::Chain& chain : debug.chains) {
        for (const Builder::Ring& ring : chain.rings) {
            const int resolution = ring.radii.size();
            REQUIRE(ring.vertices.size() == resolution);
            for (int bin = 0; bin < resolution; ++bin) {
                const QVector3D unpulled = ring.center + binDirection(ring, bin) * static_cast<float>(ring.radii.at(bin))
                                           + ring.frame.tangent * static_cast<float>(ring.offsetsAlong.at(bin));
                INFO("ring at (" << ring.center.x() << ", " << ring.center.y() << ", " << ring.center.z() << ") bin " << bin);
                CHECK(ring.vertices.at(bin).distanceToPoint(unpulled) < 1e-4f);
            }
        }
    }
}

// The facings the builder reads the leg's records along, at its from and to
// stations.
std::array<QVector3D, 2> legFacings(const Builder::Debug& debug, int legIndex)
{
    return cwPassageTubeGeometry::legEndFacings(debug.legs, debug.chains).at(legIndex);
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

int warningCount(const Builder::Debug& debug, cwPassageTubeMesh::Warning::Type type)
{
    return static_cast<int>(std::count_if(debug.warnings.cbegin(), debug.warnings.cend(), [type](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == type;
    }));
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

TEST_CASE("cwPassageTubeBuilder builds one chain per run of a chunk's shots", "[PassageTube][cwPassageTubeBuilder]")
{
    // The arm b-d is surveyed between the two halves of the passage a-b-c,
    // so each of the three chunks is its own chain, walked in chunk order.
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0)};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0)};
    const StationInput d {QStringLiteral("d"), QVector3D(10, 10, 0)};
    const StationInput unpositioned {QStringLiteral("x")};
    const StationInput e {QStringLiteral("e"), QVector3D(30, 0, 0)};
    const StationInput f {QStringLiteral("f"), QVector3D(40, 0, 0)};
    const BuilderInput input = makeInput({{{a, b}, {b, d}, {b, c, unpositioned, e, f}}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 4);
    REQUIRE(debug.chains.size() == 4);
    checkChainsCoverLegs(debug);
    CHECK(chainStations(debug, debug.chains.at(0)) == QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    CHECK(chainStations(debug, debug.chains.at(1)) == QStringList({QStringLiteral("b"), QStringLiteral("d")}));
    CHECK(chainStations(debug, debug.chains.at(2)) == QStringList({QStringLiteral("b"), QStringLiteral("c")}));
    CHECK(chainStations(debug, debug.chains.at(3)) == QStringList({QStringLiteral("e"), QStringLiteral("f")}));
    for (const Builder::Chain& chain : debug.chains) {
        CHECK_FALSE(chain.legs.first().reversed);
        CHECK(chain.startCap == Builder::EndCap::Fan);
        CHECK(chain.endCap == Builder::EndCap::Fan);
    }
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
    const StationInput c {QStringLiteral("c"), QVector3D(11, 8, 0)};
    const StationInput d {QStringLiteral("d"), QVector3D(1, 9, 0)};
    const BuilderInput input = makeInput({{{a, b, c, d, a}}});

    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.legs.size() == 4);
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
        checkVectorNear(ring.frame.tangent, QVector3D(1, 0, 0));
        checkVectorNear(ring.frame.up, QVector3D(0, 0, 1));
        checkVectorNear(ring.frame.right, QVector3D(0, -1, 0));
        CHECK(ring.radii.at(binAt(0)) == Catch::Approx(2.0));
        CHECK(ring.radii.at(binAt(90)) == Catch::Approx(3.0));
        CHECK(ring.radii.at(binAt(180)) == Catch::Approx(1.0));
        CHECK(ring.radii.at(binAt(270)) == Catch::Approx(4.0));
        CHECK_FALSE(ring.measured.contains(false));
    }

    // Between the axes a bin is the point of the ellipse of the two adjacent
    // dimensions at eccentric anomaly 45°: (2·cos45°, 3·sin45°).
    const Builder::Ring& start = chain.rings.at(0);
    CHECK(start.radii.at(binAt(45)) == Catch::Approx(std::hypot(2.0, 3.0) / std::numbers::sqrt2));
    CHECK(start.binAngles.at(binAt(45)) == Catch::Approx(std::atan2(3.0, 2.0)));
    CHECK(start.binAngles.at(binAt(0)) == 0.0);
    for (int bin = 1; bin < start.binAngles.size(); ++bin) {
        CHECK(start.binAngles.at(bin) > start.binAngles.at(bin - 1));
    }
    CHECK(debug.warnings.isEmpty());
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

// The 90 degree bend on legs of legLength meters, turning left at b.
BuilderInput rightAngleBendInput(double radius, float legLength)
{
    const QStringList dimensions = lrud(radius, radius, radius, radius);
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(10.0f - legLength, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
        {QStringLiteral("c"), QVector3D(10, legLength, 0), dimensions},
    }}});
}

const QVector3D kBendCorner(10, 0, 0);
// The sharpest bend a chain runs through, short of a hairpin.
constexpr double kSharpestBendDegrees = 95.0;
const QVector3D kSharpestBendOutgoing(static_cast<float>(std::cos(qDegreesToRadians(kSharpestBendDegrees))),
                                      static_cast<float>(std::sin(qDegreesToRadians(kSharpestBendDegrees))),
                                      0.0f);

BuilderInput sharpestBendInput()
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), kBendCorner, dimensions},
        {QStringLiteral("c"), kBendCorner + kSharpestBendOutgoing * 10.0f, dimensions},
    }}});
}

// A loop whose corners turn less than a hairpin, so one chain runs round it.
BuilderInput loopInput(const QStringList& dimensions = {QString(), QString(), QString(), QString()})
{
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
        {QStringLiteral("c"), QVector3D(11, 8, 0), dimensions},
        {QStringLiteral("d"), QVector3D(1, 9, 0), dimensions},
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
    }}});
}

} // namespace

TEST_CASE("cwPassageTubeBuilder stands a vertical pit as a box padded along the incoming passage with shaft rings off", "[PassageTube][cwPassageTubeBuilder]")
{
    // The pit b→c has no plan run, so e = 1 and its rings stand pad =
    // mean(L, R) / 2 = 0.5 m off b and c along the incoming passage, north:
    // the chain splits at both into the two level legs and the box.
    const Builder::Debug debug = buildDebug(verticalPitInput(), verticalRingOptions());
    REQUIRE(debug.chains.size() == 3);
    for (const Builder::Chain& chain : debug.chains) {
        checkFramesOrthonormal(chain);
        for (const Builder::Ring& ring : chain.rings) {
            checkVectorNear(ring.frame.up, QVector3D(0, 0, 1));
        }
    }
    checkMeshClosed(debug.mesh);

    const Builder::Chain& pit = debug.chains.at(1);
    REQUIRE(pit.legs.size() == 1);
    REQUIRE(pit.legPads.size() == 1);
    CHECK(pit.legPads.at(0).e == Catch::Approx(1.0));
    checkVectorNear(pit.rings.first().center, QVector3D(0, 9.5, 0));
    checkVectorNear(pit.rings.last().center, QVector3D(0, 10.5, -10));
    for (const Builder::Ring& ring : pit.rings) {
        checkVectorNear(ring.frame.tangent, QVector3D(0, 1, 0));
        CHECK_FALSE(ring.measured.contains(false));
        for (double radius : ring.radii) {
            CHECK(radius == Catch::Approx(1.0));
        }
    }

    const Builder::Ring& upper = ringAt(debug.chains.at(0), QVector3D(0, 5, 0));
    CHECK_FALSE(upper.measured.contains(false));
    ringAt(debug.chains.at(2), QVector3D(0, 15, -10));
}

TEST_CASE("cwPassageTubeBuilder keeps a pit left on a new bearing vertical", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(pitOnNewBearingInput());
    REQUIRE(debug.chains.size() == 3);
    for (const Builder::Chain& chain : debug.chains) {
        checkFramesOrthonormal(chain);
    }
    checkMeshClosed(debug.mesh);

    const Builder::Ring& exit = ringAt(debug.chains.last(), QVector3D(10, 10, -10));
    checkVectorNear(exit.frame.up, QVector3D(0, 0, 1));
    checkVectorNear(exit.frame.right, QVector3D(0, -1, 0));
}

TEST_CASE("cwPassageTubeBuilder stands a chain that is vertical throughout as two shafts", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0)},
        {QStringLiteral("b"), QVector3D(0, 0, -10)},
        {QStringLiteral("c"), QVector3D(0, 0, -20)},
    }}});

    // Each leg drops 10 m in a section 2 m tall: a shaft, its own piece of
    // the chain, its rings square to the plumb leg and centered on it.
    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 2);
    for (const Builder::Chain& chain : debug.chains) {
        checkFramesOrthonormal(chain);
        REQUIRE(chain.legPads.size() == 1);
        CHECK(chain.legPads.at(0).isShaft);
        for (const Builder::Ring& ring : chain.rings) {
            checkVectorNear(ring.frame.tangent, QVector3D(0, 0, -1));
            checkVectorNear(QVector3D(ring.center.x(), ring.center.y(), 0), QVector3D(0, 0, 0));
        }
    }
    checkMeshClosed(debug.mesh);
}

TEST_CASE("cwPassageTubeBuilder keeps a ring's world shape whichever way the chunk is entered", "[PassageTube][cwPassageTubeBuilder]")
{
    // The same passage surveyed both ways; stored LRUD follow the chunk's
    // station order, so left and right trade places. The backward survey's
    // chunk b→a reads b facing west.
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
    REQUIRE(backward.chains.size() == 2);
    CHECK(chainStations(forward, forward.chains.at(0)) == QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
    CHECK(chainStations(backward, backward.chains.at(0)) == QStringList({QStringLiteral("b"), QStringLiteral("a")}));

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

TEST_CASE("cwPassageTubeBuilder takes a wall at face value across the ring", "[PassageTube][cwPassageTubeBuilder]")
{
    // b faces the bisector of a→b and b→c, (1, 1, 0)/√2, so its one ring
    // lays b's 5 m left wall its full length along the facing's left (T07ah
    // rule 1). The chunk b→d reads b on its own.
    constexpr float kLeftWall = 5.0f;
    constexpr float kToleranceMeters = 0.2f;
    const QStringList ends = lrud(1, 1, 1, 1);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), ends};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), lrud(5, 0, 1, 1)};
    const StationInput c {QStringLiteral("c"), QVector3D(10, 10, 0), ends};
    const StationInput d {QStringLiteral("d"), QVector3D(10, -10, 0)};
    const StationInput bTall {QStringLiteral("b"), QVector3D(10, 0, 0), lrud(1, 1, 3, 3)};
    const BuilderInput input = makeInput({{{bTall, d}, {a, b, c}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 2);
    const Builder::Chain& passage = debug.chains.at(1);
    CHECK(chainStations(debug, passage) == QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));

    const QVector3D bPosition = b.position.value();
    const QVector3D facingLeft = QVector3D(-1, 1, 0).normalized();
    const auto atB = [&](const Builder::Ring& ring) {
        return ring.center == bPosition;
    };
    CHECK(std::count_if(passage.rings.cbegin(), passage.rings.cend(), atB) == 1);
    const auto ringAtB = std::find_if(passage.rings.cbegin(), passage.rings.cend(), atB);
    REQUIRE(ringAtB != passage.rings.cend());
    checkFarthestVertexNear(*ringAtB, bPosition + facingLeft * kLeftWall, kToleranceMeters);
    checkMeshClosed(debug.mesh);
}

TEST_CASE("cwPassageTubeBuilder falls back to the next station's facing on a vertical shot", "[PassageTube][cwPassageTubeBuilder]")
{
    // b's leg in chunk 2 drops straight down, so b takes q's facing (the
    // bisector of the drop and q→r, west). With shaft rings off, the drop has
    // no plan run, so its rings stand pad = mean(L, R) / 2 off b and q along
    // the outgoing plan direction, west, with b's 3 m left wall to the south
    // and its 0 m right wall to the north: the chain splits at q.
    constexpr double kLeftMeters = 3.0;
    constexpr double kPadMeters = (kLeftMeters + Builder::kMinRadiusMeters) / 2.0 / 2.0;
    constexpr double kUpMeters = 1.0;
    const QVector3D bPosition(10, 0, 0);
    const QVector3D west(-1, 0, 0);
    const BuilderInput input = makeInput({{
        {
            {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(1, 1, 1, 1)},
            {QStringLiteral("b"), bPosition},
        },
        {
            {QStringLiteral("b"), bPosition, lrud(kLeftMeters, 0, kUpMeters, kUpMeters)},
            {QStringLiteral("q"), QVector3D(10, 0, -10)},
            {QStringLiteral("r"), QVector3D(0, 0, -10)},
        },
    }});

    const Builder::Debug debug = buildDebug(input, verticalRingOptions());
    const auto drop = std::find_if(debug.legs.cbegin(), debug.legs.cend(), [](const Builder::Leg& leg) {
        return leg.from == QStringLiteral("b") && leg.to == QStringLiteral("q");
    });
    REQUIRE(drop != debug.legs.cend());
    const auto [fromFacing, toFacing] = legFacings(debug, static_cast<int>(drop - debug.legs.cbegin()));
    checkVectorNear(fromFacing, west);
    checkVectorNear(toFacing, west);

    REQUIRE(debug.chains.size() == 3);
    const Builder::Chain& shaft = debug.chains.at(1);
    REQUIRE(shaft.legs.size() == 1);
    const Builder::Ring& shaftTop = shaft.rings.first();
    checkVectorNear(shaftTop.center, bPosition - west * static_cast<float>(kPadMeters));
    checkVectorNear(shaftTop.frame.up, QVector3D(0, 0, 1));
    checkVectorNear(shaftTop.frame.right, QVector3D(0, 1, 0));
    const auto [south, north] = std::minmax_element(shaftTop.vertices.cbegin(), shaftTop.vertices.cend(), [](const QVector3D& p, const QVector3D& q) {
        return p.y() < q.y();
    });
    const auto [lowest, highest] = std::minmax_element(shaftTop.vertices.cbegin(), shaftTop.vertices.cend(), [](const QVector3D& p, const QVector3D& q) {
        return p.z() < q.z();
    });
    CHECK(south->y() == Catch::Approx(-kLeftMeters));
    CHECK(north->y() == Catch::Approx(Builder::kMinRadiusMeters));
    CHECK(lowest->z() == Catch::Approx(-kUpMeters));
    CHECK(highest->z() == Catch::Approx(kUpMeters));
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

    // The ring 9/10 of the way along b→c lies on the loft between b's
    // borrowed section and c's.
    constexpr double kNearCRadius = 1.0 + (2.0 - 1.0) * 0.9;
    const Builder::Ring& nearC = ringAt(chain, QVector3D(13, 0, 0));
    CHECK_FALSE(hasMeasuredBin(nearC));
    for (double radius : nearC.radii) {
        CHECK(radius == Catch::Approx(kNearCRadius));
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
        REQUIRE(ring.radii.size() == options.ringResolution);
        for (double radius : ring.radii) {
            CHECK(radius == Catch::Approx(options.defaultRadiusMeters));
        }
        CHECK_FALSE(hasMeasuredBin(ring));
    }

    REQUIRE(debug.warnings.size() == 1);
    CHECK(debug.warnings.at(0).type == cwPassageTubeMesh::Warning::NoPassageData);
    CHECK(debug.warnings.at(0).station == QStringLiteral("a"));
}

TEST_CASE("cwPassageTubeBuilder stands a lone pit on its own readings", "[PassageTube][cwPassageTubeBuilder]")
{
    // The top reads the drop below it and the bottom the shaft above it, so
    // the pit rises less than its section is tall and is padded, not a
    // shaft. It has no plan run, so e = 1 and its two rings stand pad =
    // mean(L, R) = 1.5 m off the stations along north, the fallback facing:
    // a box 3 m deep.
    constexpr double kPitRadius = 1.5;
    constexpr double kDropMeters = 12.0;
    constexpr double kLipMeters = 1.0;
    const BuilderInput input = makeInput({{{
        {QStringLiteral("top"), QVector3D(0, 0, 0), lrud(kPitRadius, kPitRadius, kLipMeters, kDropMeters)},
        {QStringLiteral("bottom"), QVector3D(0, 0, -kDropMeters), lrud(kPitRadius, kPitRadius, kDropMeters, kLipMeters)},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    const float pad = static_cast<float>(kPitRadius);
    checkVectorNear(chain.rings.first().center, QVector3D(0, -pad, 0));
    checkVectorNear(chain.rings.last().center, QVector3D(0, pad, -kDropMeters));
    for (const Builder::Ring& ring : chain.rings) {
        INFO("ring at z " << ring.center.z());
        CHECK_FALSE(ring.measured.contains(false));
        checkVectorNear(ring.frame.up, QVector3D(0, 0, 1));
        CHECK(ring.radii.at(binAt(0)) == Catch::Approx(kPitRadius).margin(1e-3));
        CHECK(ring.radii.at(binAt(180)) == Catch::Approx(kPitRadius).margin(1e-3));
    }
    CHECK(debug.warnings.isEmpty());
    checkMeshClosed(debug.mesh);
}

TEST_CASE("cwPassageTubeBuilder pads a lone plumb leg along its shot's bearing", "[PassageTube][cwPassageTubeBuilder]")
{
    // The chain is one plumb leg, rising less than its section is tall, so
    // its plan direction is the bearing its shot reads: east by every one of
    // these readings. Its rings stand pad = mean(L, R) = 3 m west of the top
    // and east of the bottom, with right to the south.
    struct Reading {
        const char* name;
        std::optional<double> frontsight;
        std::optional<double> backsight;
        bool isFrontsightCorrected = false;
        bool isBacksightCorrected = false;
    };
    const Reading reading = GENERATE(Reading {"frontsight", 90.0, std::nullopt},
                                     Reading {"corrected frontsight", 270.0, std::nullopt, true},
                                     Reading {"backsight", std::nullopt, 270.0},
                                     Reading {"corrected backsight", std::nullopt, 90.0, false, true});
    INFO(reading.name);

    constexpr double kPitRadius = 3.0;
    constexpr double kDropMeters = 12.0;
    constexpr double kLipMeters = 1.0;
    BuilderInput input = makeInput({{{
        {QStringLiteral("top"), QVector3D(0, 0, 0), lrud(kPitRadius, kPitRadius, kLipMeters, kDropMeters)},
        {QStringLiteral("bottom"), QVector3D(0, 0, -kDropMeters), lrud(kPitRadius, kPitRadius, kDropMeters, kLipMeters)},
    }}});
    cwTripData& trip = input.region.caves.first().trips.first();
    trip.calibrations.setCorrectedCompassFrontsight(reading.isFrontsightCorrected);
    trip.calibrations.setCorrectedCompassBacksight(reading.isBacksightCorrected);
    cwShot& shot = trip.chunks.first().shots.first();
    shot.setCompass(reading.frontsight ? cwCompassReading(*reading.frontsight) : cwCompassReading());
    if (reading.backsight) {
        shot.setBackCompass(cwCompassReading(*reading.backsight));
    }

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    const float pad = static_cast<float>(kPitRadius);
    checkVectorNear(chain.rings.first().center, QVector3D(-pad, 0, 0));
    checkVectorNear(chain.rings.last().center, QVector3D(pad, 0, -kDropMeters));
    for (const Builder::Ring& ring : chain.rings) {
        INFO("ring at z " << ring.center.z());
        checkVectorNear(ring.frame.up, QVector3D(0, 0, 1));
        checkVectorNear(ring.frame.right, QVector3D(0, -1, 0));
    }
    checkMeshClosed(debug.mesh);
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

TEST_CASE("cwPassageTubeBuilder spreads bins by arc length on a thin section", "[PassageTube][cwPassageTubeBuilder]")
{
    // At uniform polar angles the bins at 67.5° and 90° of this section land
    // at (0.77, 1.86) and (0, 7), one vertex for over 5 m of wall. Bins at
    // uniform eccentric anomaly are no farther apart than the bins of a
    // circle of the 7 m semi-axis. A bound of 1.2 times the mean spacing is
    // out of reach while each quadrant keeps N/4 bins with the wall readings
    // on its axis bins: the 7 m quadrants hold over 80% of the perimeter, so
    // even equal arcs per quadrant leave the largest gap 1.67 times the mean
    // (2.5 times at eccentric anomaly).
    constexpr double kHalfWidth = 0.8;
    constexpr double kCeiling = 7.0;
    const QStringList dimensions = lrud(kHalfWidth, kHalfWidth, kCeiling, 1);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Ring& ring = debug.chains.at(0).rings.at(1);
    const int resolution = ring.vertices.size();
    const double maxGap = 2.0 * kCeiling * std::sin(std::numbers::pi / resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        INFO("bin " << bin);
        CHECK(ring.vertices.at(bin).distanceToPoint(ring.vertices.at((bin + 1) % resolution)) <= maxGap);
    }
    CHECK(ring.radii.at(binAt(90)) == Catch::Approx(kCeiling));
    CHECK(ring.binAngles.at(binAt(90)) == Catch::Approx(std::numbers::pi / 2.0));
    checkMeshClean(debug);
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
    // Two rings of vertices, each closed by a fan around its center.
    CHECK(mesh.positions.size() == 2 * resolution + 2);
    // Two side triangles per bin plus a fan triangle per bin at each end.
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

namespace {

const cwPassageTubeCases::Case& catalogCase(const QString& name)
{
    static const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();
    const auto found = std::find_if(catalog.cbegin(), catalog.cend(), [&](const cwPassageTubeCases::Case& entry) {
        return entry.name == name;
    });
    REQUIRE(found != catalog.cend());
    return *found;
}

Builder::Debug buildCase(const cwPassageTubeCases::Case& entry)
{
    const BuilderInput input = cwPassageTubeCases::toSurvey(entry);
    Builder::Options options;
    options.sampleSpacingMeters = entry.sampleSpacing;
    return Builder::buildDebug(input.region, input.positionsByCave, {}, options);
}

QVector3D casePosition(const cwPassageTubeCases::Case& entry, const QString& name)
{
    for (const QList<StationInput>& chunk : entry.trip.chunks) {
        for (const StationInput& station : chunk) {
            if (station.name == name && station.position) {
                return *station.position;
            }
        }
    }
    FAIL("no station " << name.toStdString());
    return {};
}

} // namespace

namespace {

constexpr float kSameStationMeters = 1e-4f;

// A station's left and right reading points as a leg walked along direction
// sees them: the station plus each side reading along its record's right
// direction, the facing turned clockwise.
struct ReadingPoints {
    std::optional<QVector3D> left;
    std::optional<QVector3D> right;
};

ReadingPoints readingPoints(const cwStationLrud& lrud, const QVector3D& facing, const QVector3D& station, const QVector3D& direction)
{
    const QVector3D worldUp(0.0f, 0.0f, 1.0f);
    QVector3D horizontalFacing = facing;
    horizontalFacing.setZ(0.0f);
    horizontalFacing.normalize();

    const bool facesAgainst = QVector3D::dotProduct(facing, direction) < 0.0f;
    const QVector3D right = QVector3D::crossProduct(horizontalFacing, worldUp) * (facesAgainst ? -1.0f : 1.0f);
    const auto point = [&](const std::optional<double>& reading, float sign) -> std::optional<QVector3D> {
        if (reading) {
            return station + right * (sign * static_cast<float>(*reading));
        }
        return std::nullopt;
    };
    return {point(facesAgainst ? lrud.right : lrud.left, -1.0f), point(facesAgainst ? lrud.left : lrud.right, 1.0f)};
}

} // namespace

// b–c and c–d are the second trip's chain, read only at d; b's ring, read
// in the first trip, lies nearer the rings on b–c but they borrow d's.
TEST_CASE("cwPassageTubeBuilder borrows a ring from its own trip before a nearer one across a trip boundary", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kNarrow = 0.2;
    constexpr double kWide = 2.0;
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(kNarrow, kNarrow, kNarrow, kNarrow)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), lrud(kNarrow, kNarrow, kNarrow, kNarrow)};
    const StationInput bUnread {QStringLiteral("b"), QVector3D(10, 0, 0)};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0)};
    const StationInput d {QStringLiteral("d"), QVector3D(30, 0, 0), lrud(kWide, kWide, kWide, kWide)};
    const SurveyInput first {{{a, b}}};
    const SurveyInput second {{{bUnread, c, d}}};
    const Builder::Debug debug = buildDebug(cwPassageTubeCases::toSurvey(QList<SurveyInput> {first, second}));
    REQUIRE(debug.chains.size() == 2);

    const Builder::Ring& ring = ringAt(debug.chains.at(1), QVector3D(12, 0, 0));
    for (double radius : ring.radii) {
        CHECK(radius == Catch::Approx(kWide));
    }
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

// The arm walks d → b, south into the through tube, and ends at b, where
// the union opens it into the through.
const Builder::Chain& junctionArm(const Builder::Debug& debug)
{
    REQUIRE(debug.chains.size() == 2);
    const Builder::Chain& arm = debug.chains.at(1);
    REQUIRE(chainStations(debug, arm) == QStringList({QStringLiteral("d"), QStringLiteral("b")}));
    CHECK(arm.startCap == Builder::EndCap::Fan);
    CHECK(arm.endCap == Builder::EndCap::Fan);
    return arm;
}

} // namespace

TEST_CASE("cwPassageTubeBuilder opens a junction arm into the through passage", "[PassageTube][cwPassageTubeBuilder]")
{
    // b's 2 m walls make the through a 2 m cylinder about the x axis, which
    // holds the arm's default-radius cap at b: the union removes it.
    const Builder::Debug debug = buildDebug(junctionInput(lrud(2, 2, 2, 2)));
    junctionArm(debug);
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder merges an arm into a junction without readings", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(junctionInput({QString(), QString(), QString(), QString()}));
    junctionArm(debug);
    checkMeshClean(debug);
}

namespace {

// The mesh drawn while only running trip trip is visible: its triangles and
// the boundary discs that close it against the hidden trips.
cwPassageTubeMesh tripAlone(const cwPassageTubeMesh& mesh, int trip)
{
    cwPassageTubeMesh alone;
    alone.positions = mesh.positions;
    const auto append = [&](const cwPassageTubeMesh::VertexRange& range) {
        alone.indices.append(mesh.indices.mid(range.first, range.count));
    };
    append(mesh.tripIndexRanges.at(trip));
    for (const cwPassageTubeMesh::TripBoundary& boundary : mesh.tripBoundaryRanges) {
        if (boundary.trip == trip) {
            append(boundary.range);
        }
    }
    return alone;
}

void checkNoCapExposed(const Builder::Debug& debug)
{
    const QVector<cwPassageTubeMeshCheck::Issue> issues = cwPassageTubeMeshCheck::capsExposed(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText({issues, 0, {}}).toStdString());
    CHECK(issues.isEmpty());
}

void checkCenterlineInside(const Builder::Debug& debug)
{
    const QVector<cwPassageTubeMeshCheck::Issue> issues = cwPassageTubeMeshCheck::centerlineInside(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText({issues, 0, {}}).toStdString());
    CHECK(issues.isEmpty());
}

// The stations of the caps drawn with every trip visible, sorted.
QStringList visibleCapStations(const Builder::Debug& debug)
{
    const cwPassageTubeMesh::VertexRange visible = cwPassageTubeMeshCheck::allTripsVisible(debug.mesh);
    QSet<int> caps;
    for (int triangle = visible.first / 3; triangle < (visible.first + visible.count) / 3; ++triangle) {
        if (debug.triangleCap.at(triangle) >= 0) {
            caps.insert(debug.triangleCap.at(triangle));
        }
    }
    QStringList stations;
    for (int cap : std::as_const(caps)) {
        stations.append(debug.caps.at(cap).station);
    }
    stations.sort();
    return stations;
}

// The direction plan degrees counter-clockwise from +x.
QVector3D planDirection(double degrees)
{
    return QVector3D(static_cast<float>(std::cos(qDegreesToRadians(degrees))),
                     static_cast<float>(std::sin(qDegreesToRadians(degrees))),
                     0.0f);
}

} // namespace

// a–b–c along +x, a→b in the first trip and b→c in the second: two chains
// that end on b. The second trip reads b as wide as the first, or twice as
// wide.
TEST_CASE("cwPassageTubeBuilder continues a passage across a trip boundary without a cap", "[PassageTube][cwPassageTubeBuilder]")
{
    const double secondTripScale = GENERATE(1.0, 2.0);
    INFO("second trip reads b x" << secondTripScale);
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions};
    const StationInput bSecond {QStringLiteral("b"), QVector3D(10, 0, 0),
                                lrud(secondTripScale, secondTripScale, secondTripScale, secondTripScale)};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), dimensions};
    const BuilderInput input = cwPassageTubeCases::toSurvey(QList<SurveyInput> {{{{a, b}}}, {{{bSecond, c}}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 2);

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
    CHECK(cwPassageTubeMeshCheck::allTripsVisible(mesh).count == next);

    REQUIRE(mesh.tripBoundaryRanges.size() == 2);
    for (const cwPassageTubeMesh::TripBoundary& boundary : mesh.tripBoundaryRanges) {
        CHECK(boundary.range.first == next);
        CHECK(boundary.range.count > 0);
        CHECK(boundary.trip + boundary.neighborTrip == 1);
        next = boundary.range.first + boundary.range.count;
    }
    CHECK(next == mesh.indices.size());

    // Each trip's chain reads its own record at b, so where the second
    // reads b wider its cap at b is the step between the two.
    const QStringList steppedCaps {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    CHECK(visibleCapStations(debug) == (secondTripScale == 1.0 ? QStringList({QStringLiteral("a"), QStringLiteral("c")}) : steppedCaps));
    checkNoCapExposed(debug);
    checkCenterlineInside(debug);
    checkMeshClosed(mesh);
    for (int trip = 0; trip < mesh.tripIndexRanges.size(); ++trip) {
        INFO("trip " << trip << " alone");
        const cwPassageTubeMesh alone = tripAlone(mesh, trip);
        const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(alone, cwPassageTubeMeshCheck::wholeMesh(alone));
        INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
        CHECK(report.issues.isEmpty());
    }
    if (secondTripScale == 1.0) {
        checkMeshClean(debug);
    }
}

// a–b in the first trip, b–c and an arm from d, north of b, in the second:
// the through crosses trips at the junction.
TEST_CASE("cwPassageTubeBuilder keeps each trip's triangles where a passage crosses trips at a junction", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const QVector3D bPosition(10, 0, 0);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions};
    const StationInput b {QStringLiteral("b"), bPosition, dimensions};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), dimensions};
    const StationInput d {QStringLiteral("d"), QVector3D(10, 10, 0), dimensions};
    const BuilderInput input = cwPassageTubeCases::toSurvey(QList<SurveyInput> {{{{a, b}}}, {{{b, c}, {d, b}}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 3);
    const cwPassageTubeMesh& mesh = debug.mesh;
    const QList<cwTripData>& trips = input.region.caves.at(0).trips;
    REQUIRE(mesh.tripUuids == QVector<QUuid>({trips.at(0).id, trips.at(1).id}));
    REQUIRE(mesh.tripIndexRanges.size() == 2);

    const auto chainHasTrip = [&](int chain, const QUuid& tripId) {
        const QVector<Builder::ChainLeg>& legs = debug.chains.at(chain).legs;
        return std::any_of(legs.cbegin(), legs.cend(), [&](const Builder::ChainLeg& chainLeg) {
            return debug.legs.at(chainLeg.legIndex).tripId == tripId;
        });
    };
    const auto isThrough = [&](int chain) { return chainStations(debug, debug.chains.at(chain)).contains(QStringLiteral("a")); };
    // The through's triangles of each trip lie on that trip's side of b's
    // ring.
    constexpr float kRingTolerance = 1e-3f;
    for (int trip = 0; trip < mesh.tripIndexRanges.size(); ++trip) {
        const cwPassageTubeMesh::VertexRange& range = mesh.tripIndexRanges.at(trip);
        CHECK(range.count > 0);
        for (int index = range.first; index < range.first + range.count; index += 3) {
            const int triangle = index / 3;
            const int chain = debug.triangleChain.at(triangle);
            INFO("trip " << trip << " triangle " << triangle << " chain " << chain);
            CHECK(chainHasTrip(chain, mesh.tripUuids.at(trip)));
            if (isThrough(chain)) {
                const QVector3D centroid = (mesh.positions.at(mesh.indices.at(index)) + mesh.positions.at(mesh.indices.at(index + 1))
                                            + mesh.positions.at(mesh.indices.at(index + 2))) / 3.0f;
                CHECK((trip == 0 ? bPosition.x() - centroid.x() : centroid.x() - bPosition.x()) > -kRingTolerance);
            }
        }
    }

    // Each trip has a disc at b that closes it while the other is hidden.
    REQUIRE(mesh.tripBoundaryRanges.size() == 2);
    for (const cwPassageTubeMesh::TripBoundary& boundary : mesh.tripBoundaryRanges) {
        CHECK(boundary.trip + boundary.neighborTrip == 1);
        REQUIRE(boundary.range.count > 0);
        for (int index = boundary.range.first; index < boundary.range.first + boundary.range.count; index += 3) {
            const int cap = debug.triangleCap.at(index / 3);
            REQUIRE(cap >= 0);
            CHECK(debug.caps.at(cap).station == QStringLiteral("b"));
        }
    }
    CHECK_FALSE(visibleCapStations(debug).contains(QStringLiteral("b")));
    for (int trip = 0; trip < mesh.tripIndexRanges.size(); ++trip) {
        INFO("trip " << trip << " alone");
        const cwPassageTubeMesh alone = tripAlone(mesh, trip);
        const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(alone, cwPassageTubeMeshCheck::wholeMesh(alone));
        INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
        CHECK(report.issues.isEmpty());
    }
    checkMeshClean(debug);
}

namespace {

// A square loop of tall passages a–b–c–d at the floor of a box, with a wide
// flat passage across the square above it and another below, each reached
// from a by a shaft and joining c by another: air on every side of the
// square's middle.
BuilderInput hollowBoxInput()
{
    constexpr float kSide = 10.0f;
    constexpr float kCoverHeight = 2.5f;
    const QStringList wall = lrud(1, 1, 2, 2);
    const QStringList shaft = lrud(1, 1, 1, 1);
    // Wide enough that its flat ellipse still overlaps the walls' crest at
    // the square's corners b and d, which lie farthest from its axis.
    const QStringList cover = lrud(12, 12, 1, 1);
    const QVector3D a(0, 0, 0);
    const QVector3D c(kSide, kSide, 0);
    const auto coverChunk = [&](const QString& prefix, float height) {
        const QVector3D up(0, 0, height);
        return QList<StationInput> {{QStringLiteral("a"), a, shaft},
                                    {prefix + QStringLiteral("1"), a + up, cover},
                                    {prefix + QStringLiteral("2"), c + up, cover},
                                    {QStringLiteral("c"), c, shaft}};
    };
    const QList<StationInput> loop {{QStringLiteral("a"), a, wall},
                                    {QStringLiteral("b"), QVector3D(kSide, 0, 0), wall},
                                    {QStringLiteral("c"), c, wall},
                                    {QStringLiteral("d"), QVector3D(0, kSide, 0), wall},
                                    {QStringLiteral("a"), a, wall}};
    return cwPassageTubeCases::toSurvey(SurveyInput {{loop, coverChunk(QStringLiteral("top"), kCoverHeight),
                                                      coverChunk(QStringLiteral("bottom"), -kCoverHeight)}});
}

} // namespace

TEST_CASE("cwPassageTubeBuilder drops a void that stacked passages enclose", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(hollowBoxInput());
    const auto voidWarning = std::find_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == cwPassageTubeMesh::Warning::EnclosedVoid;
    });
    REQUIRE(voidWarning != debug.warnings.cend());
    INFO(voidWarning->message.toStdString());
    CHECK(voidWarning->message.contains(QStringLiteral(" m³ at (")));

    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(debug.mesh, cwPassageTubeMeshCheck::allTripsVisible(debug.mesh));
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
    REQUIRE_FALSE(report.components.isEmpty());
    for (const cwPassageTubeMeshCheck::ComponentStats& component : report.components) {
        CHECK(component.volume > 0.0);
    }
}

// Trip 1 runs a→b→c east; trip 2 ties in at c and turns 150° in plan back
// beside b–c with the same readings.
TEST_CASE("cwPassageTubeBuilder covers a hairpin tie-in", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kHairpinDegrees = 150.0;
    constexpr float kLegMeters = 10.0f;
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const QVector3D tieIn(2 * kLegMeters, 0, 0);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions};
    const StationInput b {QStringLiteral("b"), QVector3D(kLegMeters, 0, 0), dimensions};
    const StationInput c {QStringLiteral("c"), tieIn, dimensions};
    const StationInput d {QStringLiteral("d"), tieIn + planDirection(kHairpinDegrees) * kLegMeters, dimensions};
    const Builder::Debug debug = buildDebug(cwPassageTubeCases::toSurvey(QList<SurveyInput> {{{{a, b, c}}}, {{{c, d}}}}));

    REQUIRE(debug.chains.size() == 2);
    checkChainsCoverLegs(debug);
    for (const Builder::Chain& chain : debug.chains) {
        const QStringList stations = chainStations(debug, chain);
        INFO(stations.join(QStringLiteral(" ")).toStdString());
        const bool endsAtTieIn = stations.last() == QStringLiteral("c");
        REQUIRE((endsAtTieIn || stations.first() == QStringLiteral("c")));
        CHECK((endsAtTieIn ? chain.endCap : chain.startCap) == Builder::EndCap::Fan);
    }

    // Each chain's cap at c closes the outside of the bend, where neither
    // tube reaches.
    CHECK(visibleCapStations(debug) == QStringList({QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("c"), QStringLiteral("d")}));
    checkNoCapExposed(debug);
    checkCenterlineInside(debug);
    checkMeshClean(debug);
}

// Trip 1 runs a→b→c and trip 2 b→c→d along +x, reading b and c 5 % wider.
TEST_CASE("cwPassageTubeBuilder treats a re-shot tie-in as one leg", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kResurveyScale = 1.05;
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const QStringList resurveyed = lrud(kResurveyScale, kResurveyScale, kResurveyScale, kResurveyScale);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), dimensions};
    const StationInput bAgain {QStringLiteral("b"), QVector3D(10, 0, 0), resurveyed};
    const StationInput cAgain {QStringLiteral("c"), QVector3D(20, 0, 0), resurveyed};
    const StationInput d {QStringLiteral("d"), QVector3D(30, 0, 0), dimensions};
    const BuilderInput input = cwPassageTubeCases::toSurvey(QList<SurveyInput> {{{{a, b, c}}}, {{{bAgain, cAgain, d}}}});
    const Builder::Debug debug = buildDebug(input);

    CHECK(debug.legs.size() == 3);
    REQUIRE(debug.chains.size() == 2);
    CHECK(debug.chains.at(0).legs.size() == 2);
    checkChainsCoverLegs(debug);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::RepeatedShot) == 1);

    const auto resurveyedLeg = std::find_if(debug.legs.cbegin(), debug.legs.cend(), [](const Builder::Leg& leg) {
        return leg.from == QStringLiteral("b") && leg.to == QStringLiteral("c");
    });
    REQUIRE(resurveyedLeg != debug.legs.cend());
    CHECK(resurveyedLeg->tripId == input.region.caves.at(0).trips.at(0).id);
    CHECK(*resurveyedLeg->fromLrud.left == Catch::Approx((1.0 + kResurveyScale) / 2.0));

    // The first trip's chain ends on the mean of the two readings at c and
    // the second starts on its own, 5 % wider: the step is c's cap.
    CHECK(visibleCapStations(debug) == QStringList({QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("d")}));
    checkNoCapExposed(debug);
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder keeps a cap at a true dead end", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const Builder::Debug debug = buildDebug(makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions},
        {QStringLiteral("c"), QVector3D(20, 0, 0), dimensions},
    }}}));
    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).startCap == Builder::EndCap::Fan);
    CHECK(debug.chains.at(0).endCap == Builder::EndCap::Fan);
    CHECK(visibleCapStations(debug) == QStringList({QStringLiteral("a"), QStringLiteral("c")}));
    checkNoCapExposed(debug);
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
    // A level shot pads nothing, so its rings stand on its stations (I01).
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

    // b → c is level, so its rings stand on b and c (I01), 0.03 m apart, and
    // c's merges into b's in the one chain.
    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    for (const Builder::Chain& chain : debug.chains) {
        for (int i = 1; i < chain.rings.size(); ++i) {
            INFO("ring " << i);
            CHECK(chain.rings.at(i).center.distanceToPoint(chain.rings.at(i - 1).center) >= 0.05f);
        }
    }
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder closes a loop without caps", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(loopInput());
    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).startCap == Builder::EndCap::None);
    CHECK(debug.chains.at(0).endCap == Builder::EndCap::None);
    checkMeshClosedWithSaneRings(debug);
}

TEST_CASE("cwPassageTubeBuilder keeps a loop closing at a sharp bend without folds", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(loopInput(lrud(3, 3, 3, 3)));
    REQUIRE(debug.chains.size() == 1);
    REQUIRE(debug.chains.at(0).startCap == Builder::EndCap::None);
    checkMeshClosedWithSaneRings(debug);
}

// The pits' lip and bottom stations, where a plumb section meets a level
// one, and the loop closing at a sharp bend, whose corner walls lie along
// their facings where the checker looks for them square to the bisector, are
// left to the hull caps and checker of T07ah3 and T07ah4.
TEST_CASE("cwPassageTubeBuilder keeps the inner wall of a sharp bend clean", "[PassageTube][cwPassageTubeBuilder]")
{
    const QList<std::pair<QString, BuilderInput>> bends = {
        {QStringLiteral("90 degree bend, radius 3"), rightAngleBendInput(3.0)},
        {QStringLiteral("95 degree bend"), sharpestBendInput()},
        {QStringLiteral("loop without caps"), loopInput()},
    };
    for (const auto& [name, input] : bends) {
        INFO(name.toStdString());
        const Builder::Debug debug = buildDebug(input);
        checkMeshClean(debug);
        checkNoVertexPulled(debug);
    }
}

TEST_CASE("cwPassageTubeBuilder keeps the Phake Cave a11 to a12 leg a single sane tube", "[PassageTube][cwPassageTubeBuilder]")
{
    // The largest wall readings on the leg: a11's left, a12's right, and
    // a12's 35 m down, the depth of an unsurveyed pit drawn at face value.
    constexpr double kMaxLeftMeters = 1.0;
    constexpr double kMaxRightMeters = 2.0;
    constexpr double kMaxRadiusMeters = 35.0;
    constexpr double kToleranceMeters = 1e-3;
    // The check takes about 140 ms alone in a Debug ASan build and runs up to
    // eight times slower inside the full suite, where other tests' worker
    // threads load the machine; the bound catches a check that grows by an
    // order of magnitude.
    constexpr qint64 kMaxCheckMilliseconds = 2000;

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

    checkMeshClosed(debug.mesh);

    QElapsedTimer timer;
    timer.start();
    cwPassageTubeMeshCheck::check(debug.mesh, debug);
    CHECK(timer.elapsed() < kMaxCheckMilliseconds);
}

TEST_CASE("cwPassageTubeBuilder joins the Phake Cave arms to their through passages", "[PassageTube][cwPassageTubeBuilder]")
{
    const BuilderInput input = makeInput(cwPassageTubeCases::phakeCave());
    const Builder::Debug debug = buildDebug(input);

    // The chain holding the leg between from and to.
    const auto chainWithLeg = [&](const QString& from, const QString& to) {
        for (int c = 0; c < debug.chains.size(); ++c) {
            for (const Builder::ChainLeg& chainLeg : debug.chains.at(c).legs) {
                const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
                if ((leg.from == from && leg.to == to) || (leg.from == to && leg.to == from)) {
                    return c;
                }
            }
        }
        FAIL("no leg " << from.toStdString() << " to " << to.toStdString());
        return -1;
    };

    const int through = chainWithLeg(QStringLiteral("a9"), QStringLiteral("a10"));
    CHECK(chainWithLeg(QStringLiteral("a10"), QStringLiteral("a11")) == through);
    CHECK(chainWithLeg(QStringLiteral("a13"), QStringLiteral("a10")) != through);
    checkMeshClosed(debug.mesh);
}

namespace {

// A station's section as a leg reads it: one point per bin, bin k at
// parameter 2πk/N on the quadrant ellipse through the record's reading
// points, from the walk's right toward up.
using SectionPoints = QVector<QVector3D>;

// The section of station's record as a gentle leg walked along walk reads it
// (T07ah rule 1): left and right along the record's facing turned clockwise,
// up and down plumb, every reading at face value.
SectionPoints sectionThrough(const QVector3D& station, const cwStationLrud& record, const QVector3D& facing, const QVector3D& walk)
{
    REQUIRE(record.up);
    REQUIRE(record.down);
    const ReadingPoints sides = readingPoints(record, facing, station, walk.normalized());
    REQUIRE(sides.left);
    REQUIRE(sides.right);
    const QVector3D rightArm = *sides.right - station;
    const QVector3D leftArm = station - *sides.left;
    const QVector3D worldUp(0.0f, 0.0f, 1.0f);

    const int resolution = Builder::Options().ringResolution;
    SectionPoints points;
    points.reserve(resolution);
    for (int k = 0; k < resolution; ++k) {
        const double theta = 2.0 * std::numbers::pi * k / resolution;
        const double cosine = std::cos(theta);
        const double sine = std::sin(theta);
        const QVector3D across = cosine >= 0.0 ? rightArm : leftArm;
        const double plumb = sine >= 0.0 ? *record.up : *record.down;
        points.append(station + across * static_cast<float>(cosine) + worldUp * static_cast<float>(plumb * sine));
    }
    return points;
}

SectionPoints lerpSection(const SectionPoints& start, const SectionPoints& end, double t)
{
    SectionPoints points;
    points.reserve(start.size());
    for (int k = 0; k < start.size(); ++k) {
        points.append(start.at(k) + (end.at(k) - start.at(k)) * static_cast<float>(t));
    }
    return points;
}

// How far the ring's farthest vertex lies from the section point of its bin:
// zero when the ring is the section.
double vertexMiss(const Builder::Ring& ring, const SectionPoints& section)
{
    if (ring.vertices.size() != section.size()) {
        return std::numeric_limits<double>::max();
    }
    double worst = 0.0;
    for (int k = 0; k < section.size(); ++k) {
        worst = std::max(worst, static_cast<double>(ring.vertices.at(k).distanceToPoint(section.at(k))));
    }
    return worst;
}

// A chain leg as the chain walks it, with the section each end's record of
// the leg's own chunk gives.
struct Loft {
    int chainLeg = -1;
    QString startName;
    QString endName;
    QVector3D start;
    QVector3D end;
    SectionPoints startSection;
    SectionPoints endSection;

    double fractionOf(const QVector3D& point) const
    {
        const QVector3D along = end - start;
        return std::clamp(static_cast<double>(QVector3D::dotProduct(point - start, along) / along.lengthSquared()), 0.0, 1.0);
    }
};

QString vectorText(const QVector3D& v)
{
    return QStringLiteral("(%1, %2, %3)").arg(v.x(), 0, 'f', 4).arg(v.y(), 0, 'f', 4).arg(v.z(), 0, 'f', 4);
}

bool joins(const Builder::Leg& leg, const QString& a, const QString& b)
{
    return (leg.from == a && leg.to == b) || (leg.from == b && leg.to == a);
}

// The chain holding the leg between a and b, walked either way.
const Builder::Chain& chainJoining(const Builder::Debug& debug, const QString& a, const QString& b)
{
    const auto it = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [&](const Builder::Chain& chain) {
        return std::any_of(chain.legs.cbegin(), chain.legs.cend(), [&](const Builder::ChainLeg& chainLeg) {
            return joins(debug.legs.at(chainLeg.legIndex), a, b);
        });
    });
    REQUIRE(it != debug.chains.cend());
    return *it;
}

Loft loftOf(const Builder::Debug& debug, const Builder::Chain& chain, const QString& a, const QString& b)
{
    for (int i = 0; i < chain.legs.size(); ++i) {
        const Builder::ChainLeg& chainLeg = chain.legs.at(i);
        const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
        if (!joins(leg, a, b)) {
            continue;
        }
        Loft loft;
        loft.chainLeg = i;
        loft.startName = chainLeg.reversed ? leg.to : leg.from;
        loft.endName = chainLeg.reversed ? leg.from : leg.to;
        loft.start = chainLeg.reversed ? leg.toPosition : leg.fromPosition;
        loft.end = chainLeg.reversed ? leg.fromPosition : leg.toPosition;
        const QVector3D walk = loft.end - loft.start;
        const auto [fromFacing, toFacing] = legFacings(debug, chainLeg.legIndex);
        loft.startSection = sectionThrough(loft.start, chainLeg.reversed ? leg.toLrud : leg.fromLrud, chainLeg.reversed ? toFacing : fromFacing, walk);
        loft.endSection = sectionThrough(loft.end, chainLeg.reversed ? leg.fromLrud : leg.toLrud, chainLeg.reversed ? fromFacing : toFacing, walk);
        return loft;
    }
    FAIL("no chain leg between " << a.toStdString() << " and " << b.toStdString());
    return {};
}

// Checks that every vertex k of every ring on the leg between a and b lies
// within kWallTolerance of lerp(start[k], end[k], t) at the ring's fraction t
// along the leg, and lists the rings that miss.
void checkLoftedLeg(const Builder::Debug& debug, const QString& a, const QString& b)
{
    const Builder::Chain& chain = chainJoining(debug, a, b);
    const Loft loft = loftOf(debug, chain, a, b);
    double worst = 0.0;
    int checkedRings = 0;
    QStringList misses;
    for (int i = 0; i < chain.rings.size(); ++i) {
        const Builder::Ring& ring = chain.rings.at(i);
        if (ring.chainLeg != loft.chainLeg) {
            continue;
        }
        ++checkedRings;
        const double t = loft.fractionOf(ring.center);
        const double miss = vertexMiss(ring, lerpSection(loft.startSection, loft.endSection, t));
        worst = std::max(worst, miss);
        if (miss > cwPassageTubeMeshCheck::kWallTolerance) {
            misses.append(QStringLiteral("ring %1 at t %2, %3 m off the leg: a vertex %4 m off the loft")
                              .arg(i)
                              .arg(t, 0, 'f', 3)
                              .arg(ring.center.distanceToPoint(loft.start + (loft.end - loft.start) * static_cast<float>(t)), 0, 'f', 2)
                              .arg(miss, 0, 'f', 2));
        }
    }
    INFO(loft.startName.toStdString() << " -> " << loft.endName.toStdString() << ": " << checkedRings << " rings, worst vertex "
                                      << worst << " m off the loft\n"
                                      << misses.join(QStringLiteral("\n")).toStdString());
    CHECK(checkedRings > 0);
    CHECK(worst <= cwPassageTubeMeshCheck::kWallTolerance);
}

// The ring of the loft between a and b that best matches section at
// station: a ring centered on station, or a ring on the loft's leg.
std::pair<int, double> loftEndRing(const Builder::Chain& chain, const Loft& loft, const QVector3D& station, const SectionPoints& section)
{
    std::pair<int, double> best {-1, std::numeric_limits<double>::max()};
    for (int i = 0; i < chain.rings.size(); ++i) {
        const Builder::Ring& ring = chain.rings.at(i);
        if (ring.chainLeg == loft.chainLeg || ring.center.distanceToPoint(station) < kSameStationMeters) {
            const double miss = vertexMiss(ring, section);
            if (miss < best.second) {
                best = {i, miss};
            }
        }
    }
    return best;
}

// Checks that the loft between a and b ends at station on the section of
// expected, read along the leg's own facing at station.
void checkLoftEndsOn(const Builder::Debug& debug, const QString& a, const QString& b, const QString& station, const cwStationLrud& expected)
{
    const Builder::Chain& chain = chainJoining(debug, a, b);
    const Loft loft = loftOf(debug, chain, a, b);
    const bool isStart = loft.startName == station;
    const QVector3D position = isStart ? loft.start : loft.end;
    const Builder::ChainLeg& chainLeg = chain.legs.at(loft.chainLeg);
    const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
    const bool stationIsFrom = leg.from == station;
    const auto [fromFacing, toFacing] = legFacings(debug, chainLeg.legIndex);
    const SectionPoints section = sectionThrough(position, expected, stationIsFrom ? fromFacing : toFacing, loft.end - loft.start);
    const auto [ringIndex, miss] = loftEndRing(chain, loft, position, section);
    REQUIRE(ringIndex >= 0);
    const Builder::Ring& ring = chain.rings.at(ringIndex);
    const float lowest = std::min_element(ring.vertices.cbegin(), ring.vertices.cend(), [](const QVector3D& p, const QVector3D& q) {
                             return p.z() < q.z();
                         })->z();
    INFO(a.toStdString() << " -> " << b.toStdString() << " at " << station.toStdString() << ": closest ring " << ringIndex << " (chain leg "
                         << ring.chainLeg << " of " << loft.chainLeg << "), " << ring.center.distanceToPoint(position)
                         << " m from the station, a vertex " << miss << " m off the section; lowest vertex " << position.z() - lowest
                         << " m below the station, the section's " << *expected.down);
    CHECK(ring.center.distanceToPoint(position) < kSameStationMeters);
    CHECK(miss <= cwPassageTubeMeshCheck::kWallTolerance);
    CHECK(position.z() - lowest >= *expected.down - cwPassageTubeMeshCheck::kWallTolerance);
}

} // namespace

// T07ah rule 1: a leg is the loft between its two stations' sections, the
// quadrant ellipses through their records' reading points, so every ring is
// the point-wise lerp of the two at its fraction along the leg and no elbow
// rounds a corner. Case 30's s1 reads 3 m left facing the bisector of its
// legs, so its outside corner is that reading point.
TEST_CASE("cwPassageTubeBuilder lofts each leg between its station sections", "[PassageTube][cwPassageTubeBuilder]")
{
    SECTION("S-bend")
    {
        constexpr double kS1Left = 3.0;
        const cwPassageTubeCases::Case& entry = catalogCase(QStringLiteral("30. S-bend from a small to a large to a medium section"));
        const Builder::Debug debug = buildCase(entry);
        REQUIRE(debug.chains.size() == 1);
        const Builder::Chain& chain = debug.chains.at(0);

        checkLoftedLeg(debug, QStringLiteral("s0"), QStringLiteral("s1"));
        checkLoftedLeg(debug, QStringLiteral("s1"), QStringLiteral("s2"));

        const QVector3D s1 = casePosition(entry, QStringLiteral("s1"));
        const Loft loft = loftOf(debug, chain, QStringLiteral("s1"), QStringLiteral("s2"));
        const QVector3D outside = loft.startSection.at(Builder::Options().ringResolution / 2);
        REQUIRE(outside.distanceToPoint(s1) == Catch::Approx(kS1Left));
        const Builder::Ring& ring = stationRing(chain, s1);
        const double outsideMiss = ring.vertices.isEmpty() ? std::numeric_limits<double>::max()
                                                           : ring.vertices.at(Builder::Options().ringResolution / 2).distanceToPoint(outside);
        INFO("s1's ring lies " << ring.center.distanceToPoint(s1) << " m from s1"
                               << "; its outside vertex " << outsideMiss << " m from s1's left reading point, a vertex "
                               << vertexMiss(ring, loft.startSection) << " m off s1's section");
        CHECK(ring.center.distanceToPoint(s1) < kSameStationMeters);
        CHECK(outsideMiss <= cwPassageTubeMeshCheck::kWallTolerance);
        CHECK(vertexMiss(ring, loft.startSection) <= cwPassageTubeMeshCheck::kWallTolerance);
    }

    SECTION("Phake Cave a1 to a4")
    {
        const Builder::Debug debug = buildDebug(makeInput(cwPassageTubeCases::phakeCave()));
        checkLoftedLeg(debug, QStringLiteral("a1"), QStringLiteral("a2"));
        checkLoftedLeg(debug, QStringLiteral("a2"), QStringLiteral("a3"));
        checkLoftedLeg(debug, QStringLiteral("a3"), QStringLiteral("a4"));
    }
}

// T07ah rule 4: a leg's end section comes from the record of the leg's own
// chunk. Phake's a4 reads L0 R2 U2 D1.7 in chunk 1, facing along a3 -> a5,
// and L5 R0 U3 D23 in chunk 2, facing a6: the a4 -> a6 loft starts 23 m
// deep below a4, the drop drawn at face value.
TEST_CASE("cwPassageTubeBuilder reads one record per leg end", "[PassageTube][cwPassageTubeBuilder]")
{
    const cwStationLrud chunkOneRecord {Builder::kMinRadiusMeters, 2.0, 2.0, 1.7};
    const cwStationLrud chunkTwoRecord {5.0, Builder::kMinRadiusMeters, 3.0, 23.0};
    const Builder::Debug debug = buildDebug(makeInput(cwPassageTubeCases::phakeCave()));

    SECTION("a3 -> a4 ends on chunk 1's record")
    {
        checkLoftEndsOn(debug, QStringLiteral("a3"), QStringLiteral("a4"), QStringLiteral("a4"), chunkOneRecord);
    }

    SECTION("a4 -> a6 starts on chunk 2's record")
    {
        checkLoftEndsOn(debug, QStringLiteral("a4"), QStringLiteral("a6"), QStringLiteral("a4"), chunkTwoRecord);
    }
}

// Redesign step 5: with shaft rings off, a plumb leg's rings stand vertical,
// padded e·pad off its stations along the next leg's plan direction, so the
// shaft is a box. Phake's a6 reads L1 R5 and a9 reads L2.3 R2, both facing
// 230° along a9 -> a10: pad(a6) = 1.5 m and pad(a9) = 1.075 m, and e = 1.
TEST_CASE("cwPassageTubeBuilder stands a plumb leg's rings off its stations along the next leg with shaft rings off", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kFacingAzimuthDegrees = 230.0;
    constexpr double kFacingToleranceDegrees = 1.0;
    constexpr float kA6LeftMeters = 1.0f;
    constexpr float kA6RightMeters = 5.0f;
    constexpr float kA9LeftMeters = 2.3f;
    constexpr float kA9RightMeters = 2.0f;
    constexpr float kRingCenterTolerance = 1e-3f;

    // The rings as placed, before the cap slide moves a6's into chunk 2's tube.
    const BuilderInput input = makeInput(cwPassageTubeCases::phakeCave());
    const Builder::Debug debug = Builder::buildChainRings(input.region, input.positionsByCave, verticalRingOptions());
    const QString a6Name = QStringLiteral("a6");
    const QString a9Name = QStringLiteral("a9");
    const Builder::Chain& chain = chainJoining(debug, a6Name, a9Name);
    const Loft loft = loftOf(debug, chain, a6Name, a9Name);
    const Builder::Leg& leg = debug.legs.at(chain.legs.at(loft.chainLeg).legIndex);
    const bool a6IsFrom = leg.from == a6Name;
    const QVector3D a6 = a6IsFrom ? leg.fromPosition : leg.toPosition;
    const QVector3D a9 = a6IsFrom ? leg.toPosition : leg.fromPosition;
    const auto [fromFacing, toFacing] = legFacings(debug, chain.legs.at(loft.chainLeg).legIndex);
    const QVector3D a6Facing = a6IsFrom ? fromFacing : toFacing;
    const QVector3D a9Facing = a6IsFrom ? toFacing : fromFacing;

    const auto azimuthOf = [](const QVector3D& direction) {
        const double degrees = qRadiansToDegrees(std::atan2(direction.x(), direction.y()));
        return degrees < 0.0 ? degrees + 360.0 : degrees;
    };
    INFO("a6 faces " << azimuthOf(a6Facing) << "°, a9 faces " << azimuthOf(a9Facing) << "°");
    REQUIRE(azimuthOf(a6Facing) == Catch::Approx(kFacingAzimuthDegrees).margin(kFacingToleranceDegrees));
    REQUIRE(azimuthOf(a9Facing) == Catch::Approx(kFacingAzimuthDegrees).margin(kFacingToleranceDegrees));

    const QVector3D facing = QVector3D(a6Facing.x(), a6Facing.y(), 0.0f).normalized();
    const QVector3D right(facing.y(), -facing.x(), 0.0f);
    const float a6Pad = (kA6LeftMeters + kA6RightMeters) / 4.0f;
    const float a9Pad = (kA9LeftMeters + kA9RightMeters) / 4.0f;
    const QVector3D a6Center = a6 - facing * a6Pad;
    const QVector3D a9Center = a9 + facing * a9Pad;
    const auto extremeVertex = [](const Builder::Ring& ring, const QVector3D& direction) {
        return *std::max_element(ring.vertices.cbegin(), ring.vertices.cend(), [&](const QVector3D& p, const QVector3D& q) {
            return QVector3D::dotProduct(p, direction) < QVector3D::dotProduct(q, direction);
        });
    };

    QVector<const Builder::Ring*> loftRings;
    for (const Builder::Ring& ring : chain.rings) {
        if (ring.chainLeg == loft.chainLeg && !ring.vertices.isEmpty()) {
            loftRings.append(&ring);
            checkVectorNear(ring.frame.up, QVector3D(0, 0, 1));
            checkVectorNear(ring.frame.right, right);
        }
    }
    REQUIRE(loftRings.size() >= 2);
    const Builder::Ring& a6Ring = a6IsFrom ? *loftRings.first() : *loftRings.last();
    const Builder::Ring& a9Ring = a6IsFrom ? *loftRings.last() : *loftRings.first();
    const auto text = [](const QVector3D& v) {
        return vectorText(v).toStdString();
    };
    INFO("a6 ring center " << text(a6Ring.center) << ", expected " << text(a6Center) << "; a9 ring center " << text(a9Ring.center)
                           << ", expected " << text(a9Center) << "; right " << text(a6Ring.frame.right) << "; a6 left vertex "
                           << text(extremeVertex(a6Ring, -right)));
    CHECK(a6Ring.center.distanceToPoint(a6Center) <= kRingCenterTolerance);
    CHECK(a9Ring.center.distanceToPoint(a9Center) <= kRingCenterTolerance);
    CHECK(extremeVertex(a6Ring, -right).distanceToPoint(a6Center - right * kA6LeftMeters) <= cwPassageTubeMeshCheck::kWallTolerance);
    CHECK(extremeVertex(a6Ring, right).distanceToPoint(a6Center + right * kA6RightMeters) <= cwPassageTubeMeshCheck::kWallTolerance);
    CHECK(extremeVertex(a9Ring, -right).distanceToPoint(a9Center - right * kA9LeftMeters) <= cwPassageTubeMeshCheck::kWallTolerance);
    CHECK(extremeVertex(a9Ring, right).distanceToPoint(a9Center + right * kA9RightMeters) <= cwPassageTubeMeshCheck::kWallTolerance);
}

namespace {

constexpr float kBisectorRightTolerance = 1e-4f;
constexpr float kReadingPointTolerance = 1e-3f;
constexpr double kBendLegMeters = 10.0;
constexpr double kBendLeftMeters = 2.0;
constexpr double kBendRightMeters = 1.0;
constexpr float kFlatRingNormalSquared = 1e-12f;
constexpr double kNorthDegrees = 90.0;

// The normal of the plane that best fits a ring's vertices (Newell's
// method), signed so the bins run from right toward up around it.
std::optional<QVector3D> ringFacing(const Builder::Ring& ring)
{
    QVector3D normal;
    const int count = ring.vertices.size();
    for (int k = 0; k < count; ++k) {
        normal += QVector3D::crossProduct(ring.vertices.at((k + 1) % count) - ring.center, ring.vertices.at(k) - ring.center);
    }
    if (normal.lengthSquared() < kFlatRingNormalSquared) {
        return std::nullopt;
    }
    return normal.normalized();
}

} // namespace

// T07al step 1: every bend of a chain, 0 through 180°, is mitered. Station
// 2 holds one ring whose right is the plan bisector of its two legs turned
// clockwise, with L and R at face value along it, and each leg lofts from
// its end rings without folding.
TEST_CASE("cwPassageTubeBuilder miters every bend with one ring per station", "[PassageTube][cwPassageTubeBuilder]")
{
    const double theta = GENERATE(0.0, 60.0, 120.0, 150.0, 170.0);
    const QStringList dimensions = lrud(kBendLeftMeters, kBendRightMeters, 2, 1);
    const QVector3D dIn = planDirection(kNorthDegrees);
    const QVector3D dOut = planDirection(kNorthDegrees - theta);
    const QVector3D one(0, 0, 0);
    const QVector3D two = one + dIn * static_cast<float>(kBendLegMeters);
    const QVector3D three = two + dOut * static_cast<float>(kBendLegMeters);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("1"), one, dimensions},
        {QStringLiteral("2"), two, dimensions},
        {QStringLiteral("3"), three, dimensions},
    }}});
    const Builder::Debug debug = Builder::buildChainRings(input.region, input.positionsByCave, Builder::Options());

    INFO("bend " << theta << "°: " << debug.chains.size() << " chains");
    CHECK(debug.chains.size() == 1);

    const QVector3D bisectorRight = QVector3D(dIn + dOut).normalized();
    const QVector3D expectedRight(bisectorRight.y(), -bisectorRight.x(), 0.0f);

    QVector<const Builder::Ring*> stationTwoRings;
    QStringList stationTwoRights;
    for (int c = 0; c < debug.chains.size(); ++c) {
        const Builder::Chain& chain = debug.chains.at(c);
        for (int i = 0; i < chain.rings.size(); ++i) {
            const Builder::Ring& ring = chain.rings.at(i);
            if (ring.center.distanceToPoint(two) < kSameStationMeters) {
                stationTwoRings.append(&ring);
                stationTwoRights.append(QStringLiteral("chain %1 ring %2 right %3").arg(c).arg(i).arg(vectorText(ring.frame.right)));
            }
        }
    }
    INFO(stationTwoRings.size() << " rings at station 2, expected right " << vectorText(expectedRight).toStdString() << "\n"
                                << stationTwoRights.join(QStringLiteral("\n")).toStdString());
    REQUIRE(stationTwoRings.size() == 1);

    const Builder::Ring& ring = *stationTwoRings.first();
    CHECK(ring.frame.right.distanceToPoint(expectedRight) <= kBisectorRightTolerance);
    REQUIRE_FALSE(ring.vertices.isEmpty());
    const auto extremeVertex = [&](const QVector3D& direction) {
        return *std::max_element(ring.vertices.cbegin(), ring.vertices.cend(), [&](const QVector3D& p, const QVector3D& q) {
            return QVector3D::dotProduct(p, direction) < QVector3D::dotProduct(q, direction);
        });
    };
    const QVector3D rightVertex = extremeVertex(expectedRight);
    const QVector3D leftVertex = extremeVertex(-expectedRight);
    const QVector3D expectedRightVertex = two + expectedRight * static_cast<float>(kBendRightMeters);
    const QVector3D expectedLeftVertex = two - expectedRight * static_cast<float>(kBendLeftMeters);
    INFO("right vertex " << vectorText(rightVertex).toStdString() << ", expected " << vectorText(expectedRightVertex).toStdString()
                         << "; left vertex " << vectorText(leftVertex).toStdString() << ", expected "
                         << vectorText(expectedLeftVertex).toStdString());
    CHECK(rightVertex.distanceToPoint(expectedRightVertex) <= kReadingPointTolerance);
    CHECK(leftVertex.distanceToPoint(expectedLeftVertex) <= kReadingPointTolerance);

    QStringList folds;
    const Builder::Chain& chain = debug.chains.at(0);
    for (int i = 0; i < chain.rings.size(); ++i) {
        const Builder::Ring& sample = chain.rings.at(i);
        const bool atStation = sample.center.distanceToPoint(one) < kSameStationMeters || sample.center.distanceToPoint(two) < kSameStationMeters
                               || sample.center.distanceToPoint(three) < kSameStationMeters;
        if (atStation) {
            continue;
        }
        const Builder::ChainLeg& chainLeg = chain.legs.at(sample.chainLeg);
        const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
        const QVector3D walk = (chainLeg.reversed ? leg.fromPosition - leg.toPosition : leg.toPosition - leg.fromPosition).normalized();
        const std::optional<QVector3D> facing = ringFacing(sample);
        if (!facing || QVector3D::dotProduct(*facing, walk) <= 0.0f) {
            folds.append(QStringLiteral("ring %1 on chain leg %2 faces %3 against walk %4")
                             .arg(i)
                             .arg(sample.chainLeg)
                             .arg(facing ? vectorText(*facing) : QStringLiteral("nothing"))
                             .arg(vectorText(walk)));
        }
    }
    INFO(folds.size() << " folded sample rings\n" << folds.join(QStringLiteral("\n")).toStdString());
    CHECK(folds.isEmpty());
}

// Redesign step 2, case 5: the through reads L8 at J, into the arm, where
// the arm's own record reads 1.5 m; the builder names the station once. The
// symmetric T reads J alike on both chains and stays quiet.
TEST_CASE("cwPassageTubeBuilder warns of chains reading one station's wall far apart", "[PassageTube][cwPassageTubeBuilder]")
{
    using Warning = cwPassageTubeMesh::Warning;
    const cwPassageTubeCases::Case& conflict = catalogCase(QStringLiteral("35. Through reads L8 into the arm"));
    const Builder::Debug debug = buildCase(conflict);
    REQUIRE(warningCount(debug, Warning::JunctionReadingConflict) == 1);
    const auto warning = std::find_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const Warning& candidate) {
        return candidate.type == Warning::JunctionReadingConflict;
    });
    INFO(warning->message.toStdString());
    CHECK(warning->station == QStringLiteral("J"));
    CHECK(warning->message.contains(QStringLiteral(" 8 m ")));
    CHECK(warning->message.contains(QStringLiteral(" 1.5 m ")));

    const Builder::Debug equal = buildCase(catalogCase(QStringLiteral("1. Symmetric T, equal sections")));
    CHECK(warningCount(equal, Warning::JunctionReadingConflict) == 0);
}

namespace {

constexpr double kPitchLevelLegMeters = 10.0;
constexpr double kPitchLegMeters = 20.0;
constexpr double kPitchWallMeters = 2.0;
constexpr double kPitchCeilingMeters = 2.0;
constexpr double kPitchFloorMeters = 1.0;
constexpr double kPitchDropMeters = 19.0;
constexpr float kPlumbTolerance = 1e-4f;
constexpr float kPitchRingCenterTolerance = 1e-3f;
constexpr int kListedTiltedRings = 4;
const QVector3D kPitchWorldUp(0.0f, 0.0f, 1.0f);
const QVector3D kPitchNorth(0.0f, 1.0f, 0.0f);

// Stations 2 and 3 of the pitch survey: 2 at the end of a 10 m level leg
// north from 1 at the origin, and 3 at the foot of the 20 m leg pitched
// down alphaDegrees from it.
std::pair<QVector3D, QVector3D> pitchStations(double alphaDegrees)
{
    const double alphaRadians = qDegreesToRadians(alphaDegrees);
    const QVector3D two = kPitchNorth * static_cast<float>(kPitchLevelLegMeters);
    const QVector3D three = two
                            + kPitchNorth * static_cast<float>(kPitchLegMeters * std::cos(alphaRadians))
                            - kPitchWorldUp * static_cast<float>(kPitchLegMeters * std::sin(alphaRadians));
    return {two, three};
}

// The lead-in 1 → 2 reads L2 R2 U2 D1 at 1, the lip 2 reads its drop as
// L2 R2 U2 D19, and the bottom 3 reads the shaft above as L2 R2 U19 D1.
BuilderInput pitchInput(double alphaDegrees)
{
    const auto [two, three] = pitchStations(alphaDegrees);
    return makeInput({{{
        {QStringLiteral("1"), QVector3D(0, 0, 0), lrud(kPitchWallMeters, kPitchWallMeters, kPitchCeilingMeters, kPitchFloorMeters)},
        {QStringLiteral("2"), two, lrud(kPitchWallMeters, kPitchWallMeters, kPitchCeilingMeters, kPitchDropMeters)},
        {QStringLiteral("3"), three, lrud(kPitchWallMeters, kPitchWallMeters, kPitchDropMeters, kPitchFloorMeters)},
    }}});
}

// The z-range a convex ring polygon covers on the vertical line through x,
// or nullopt when the line misses it.
std::optional<std::pair<float, float>> polygonSpanAt(const QVector<QVector3D>& polygon, float x)
{
    std::optional<std::pair<float, float>> span;
    for (int k = 0; k < polygon.size(); ++k) {
        const QVector3D& a = polygon.at(k);
        const QVector3D& b = polygon.at((k + 1) % polygon.size());
        if ((a.x() - x) * (b.x() - x) > 0.0f || a.x() == b.x()) {
            continue;
        }
        const float z = a.z() + (b.z() - a.z()) * (x - a.x()) / (b.x() - a.x());
        span = span ? std::pair {std::min(span->first, z), std::max(span->second, z)} : std::pair {z, z};
    }
    return span;
}

QString ringText(int index, const Builder::Ring& ring)
{
    return QStringLiteral("ring %1 on chain leg %2: center %3 up %4 right %5")
        .arg(index)
        .arg(ring.chainLeg)
        .arg(vectorText(ring.center))
        .arg(vectorText(ring.frame.up))
        .arg(vectorText(ring.frame.right));
}

} // namespace

// T07am step 1, redesign step 5: every ring stands vertical, L and R level
// along the step-1 right and U and D plumb. A leg's rings advance along its
// plan run, padded by e·pad at each end, e = 1 − min(1, run / (pad(a) +
// pad(b))) and pad = (|dz| / length) · mean(L, R) = 2·sin(alpha) m here
// (T07ao): a plumb pitch is a box 4 m deep along the incoming plan
// direction, as deep as it is wide, and a pitch whose plan run reaches
// 2·pad ends its rings on its stations. The pitch drops less than its
// section is tall, so it is no shaft.
TEST_CASE("cwPassageTubeBuilder stands a pitch's rings vertical along its plan run", "[PassageTube][cwPassageTubeBuilder]")
{
    const double alpha = GENERATE(90.0, 80.0, 70.0, 45.0);
    const double alphaRadians = qDegreesToRadians(alpha);
    const auto [two, three] = pitchStations(alpha);
    const BuilderInput input = pitchInput(alpha);
    const Builder::Debug debug = Builder::buildChainRings(input.region, input.positionsByCave, Builder::Options());

    const double pad = std::sin(alphaRadians) * (kPitchWallMeters + kPitchWallMeters) / 2.0;
    const double planRun = kPitchLegMeters * std::cos(alphaRadians);
    const double e = 1.0 - std::min(1.0, planRun / (pad + pad));
    const QVector3D expectedTop = two - kPitchNorth * static_cast<float>(e * pad);
    const QVector3D expectedBottom = three + kPitchNorth * static_cast<float>(e * pad);
    INFO("alpha " << alpha << "°: plan run " << planRun << " m, pad " << pad << " m, e " << e << "; " << debug.chains.size() << " chains");

    // The chain splits at station 2 only where the pitch's top ring leaves it.
    const bool splitsAtTwo = e * pad >= cwPassageTubeGeometry::kMinRingSpacingMeters;
    CHECK(debug.chains.size() == (splitsAtTwo ? 2 : 1));
    if (!splitsAtTwo) {
        REQUIRE(debug.chains.size() == 1);
        const QVector<Builder::Ring>& rings = debug.chains.first().rings;
        CHECK(std::count_if(rings.cbegin(), rings.cend(), [&](const Builder::Ring& ring) {
                  return ring.center.distanceToPoint(two) < kSameStationMeters;
              }) == 1);
    }

    const auto legIndexJoining = [&](const QString& a, const QString& b) {
        for (int i = 0; i < debug.legs.size(); ++i) {
            if (joins(debug.legs.at(i), a, b)) {
                return i;
            }
        }
        return -1;
    };
    const int levelLeg = legIndexJoining(QStringLiteral("1"), QStringLiteral("2"));
    const int steepLeg = legIndexJoining(QStringLiteral("2"), QStringLiteral("3"));
    REQUIRE(levelLeg >= 0);
    REQUIRE(steepLeg >= 0);

    const auto chainLegOf = [](const Builder::Chain& chain, int legIndex) {
        for (int i = 0; i < chain.legs.size(); ++i) {
            if (chain.legs.at(i).legIndex == legIndex) {
                return i;
            }
        }
        return -1;
    };

    // The steep leg's tube: the rings on the steep leg in the chain piece
    // holding it, with a ring it shares at station 2 or 3.
    QVector<const Builder::Ring*> steepRings;
    QStringList steepText;
    for (const Builder::Chain& chain : debug.chains) {
        const int steepChainLeg = chainLegOf(chain, steepLeg);
        if (steepChainLeg < 0) {
            continue;
        }
        for (int i = 0; i < chain.rings.size(); ++i) {
            const Builder::Ring& ring = chain.rings.at(i);
            const bool atPitchStation = ring.center.distanceToPoint(two) < kSameStationMeters || ring.center.distanceToPoint(three) < kSameStationMeters;
            if (ring.chainLeg == steepChainLeg || atPitchStation) {
                steepRings.append(&ring);
                steepText.append(ringText(i, ring));
            }
        }
    }
    REQUIRE_FALSE(steepRings.isEmpty());

    QStringList tilted;
    for (int i = 0; i < steepRings.size(); ++i) {
        const Builder::Frame& frame = steepRings.at(i)->frame;
        const bool isUpright = frame.up.distanceToPoint(kPitchWorldUp) <= kPlumbTolerance
                               && std::abs(frame.right.z()) <= kPlumbTolerance;
        if (!isUpright) {
            tilted.append(steepText.at(i));
        }
    }
    {
        INFO(tilted.size() << " of " << steepRings.size() << " steep-leg rings off plumb or with a tilted right\n"
                           << tilted.mid(0, kListedTiltedRings).join(QStringLiteral("\n")).toStdString());
        CHECK(tilted.isEmpty());
    }

    const QVector3D shot = three - two;
    const auto along = [&](const Builder::Ring* ring) {
        return QVector3D::dotProduct(ring->center - two, shot);
    };
    const auto [topRing, bottomRing] = std::minmax_element(steepRings.cbegin(), steepRings.cend(), [&](const Builder::Ring* a, const Builder::Ring* b) {
        return along(a) < along(b);
    });
    INFO("top end ring center " << vectorText((*topRing)->center).toStdString() << ", expected " << vectorText(expectedTop).toStdString()
                                << "; bottom end ring center " << vectorText((*bottomRing)->center).toStdString() << ", expected "
                                << vectorText(expectedBottom).toStdString());
    CHECK((*topRing)->center.distanceToPoint(expectedTop) <= kPitchRingCenterTolerance);
    CHECK((*bottomRing)->center.distanceToPoint(expectedBottom) <= kPitchRingCenterTolerance);

    // The lip's ring in the level leg's tube is the plumb bar U2 D19.
    const Builder::Ring* lip = nullptr;
    QStringList stationTwoText;
    for (const Builder::Chain& chain : debug.chains) {
        const int levelChainLeg = chainLegOf(chain, levelLeg);
        if (levelChainLeg < 0) {
            continue;
        }
        for (int i = 0; i < chain.rings.size(); ++i) {
            const Builder::Ring& ring = chain.rings.at(i);
            if (ring.center.distanceToPoint(two) < kSameStationMeters) {
                stationTwoText.append(ringText(i, ring));
                const bool onLevelLeg = ring.chainLeg == levelChainLeg;
                if (onLevelLeg || lip == nullptr) {
                    lip = &ring;
                }
            }
        }
    }
    INFO("rings at station 2 in the level leg's chain\n" << stationTwoText.join(QStringLiteral("\n")).toStdString());
    REQUIRE(lip != nullptr);
    REQUIRE_FALSE(lip->vertices.isEmpty());
    const auto [lowest, highest] = std::minmax_element(lip->vertices.cbegin(), lip->vertices.cend(), [](const QVector3D& p, const QVector3D& q) {
        return p.z() < q.z();
    });
    INFO("lip ring up " << vectorText(lip->frame.up).toStdString() << ", ceiling z " << highest->z() << ", floor z " << lowest->z());
    CHECK(lip->frame.up.distanceToPoint(kPitchWorldUp) <= kPlumbTolerance);
    CHECK(highest->z() == Catch::Approx(two.z() + kPitchCeilingMeters).margin(kReadingPointTolerance));
    CHECK(lowest->z() == Catch::Approx(two.z() - kPitchDropMeters).margin(kReadingPointTolerance));
}

// T07am step 4: lofted as read, the lead-in runs D1 → D19 into the lip, and
// at α = 90 the pitch is a box standing 1 m either side of the lip's plan
// position. Their union is one closed component whose volume is the box's
// plus the lead-in's less their overlap, measured here by slicing both
// lofts north to south and east to west and adding up the height they
// cover together.
TEST_CASE("cwPassageTubeBuilder unites a plumb pitch's box with its lead-in", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kVolumeTolerance = 0.02;
    constexpr float kSliceMeters = 0.01f;
    constexpr double kPlumbDegrees = 90.0;

    const BuilderInput input = pitchInput(kPlumbDegrees);
    const auto [two, three] = pitchStations(kPlumbDegrees);
    Builder::Options options;
    options.slideCaps = false;
    const Builder::Debug rings = Builder::buildChainRings(input.region, input.positionsByCave, options);
    REQUIRE(rings.chains.size() == 2);

    // The lead-in's floor at the lip is the drop, 19 m down.
    const Builder::Ring& lip = rings.chains.at(0).rings.constLast();
    const auto lowest = std::min_element(lip.vertices.cbegin(), lip.vertices.cend(), [](const QVector3D& p, const QVector3D& q) {
        return p.z() < q.z();
    });
    CHECK(lowest->z() == Catch::Approx(two.z() - kPitchDropMeters).margin(kReadingPointTolerance));

    const Builder::Debug debug = Builder::buildDebug(input.region, input.positionsByCave, {}, options);
    const cwPassageTubeMeshCheck::Report closed = cwPassageTubeMeshCheck::closed(debug.mesh, cwPassageTubeMeshCheck::allTripsVisible(debug.mesh));
    INFO(cwPassageTubeMeshCheck::toText(closed).toStdString());
    CHECK(closed.issues.isEmpty());
    CHECK(closed.componentCount == 1);

    // Each loft's end rings stand square to north, so its slice at y is the
    // lerp of the two at y's fraction between them.
    struct Loft {
        QVector<QVector3D> start;
        QVector<QVector3D> end;
    };
    QVector<Loft> lofts;
    float south = std::numeric_limits<float>::max();
    float north = std::numeric_limits<float>::lowest();
    for (const Builder::Chain& chain : rings.chains) {
        lofts.append({chain.rings.constFirst().vertices, chain.rings.constLast().vertices});
        south = std::min(south, chain.rings.constFirst().center.y());
        north = std::max(north, chain.rings.constLast().center.y());
    }
    double expected = 0.0;
    for (float y = south + kSliceMeters / 2.0f; y < north; y += kSliceMeters) {
        for (float x = static_cast<float>(-kPitchWallMeters); x < kPitchWallMeters; x += kSliceMeters) {
            QVector<std::pair<float, float>> spans;
            for (const Loft& loft : std::as_const(lofts)) {
                const float startY = loft.start.constFirst().y();
                const float endY = loft.end.constFirst().y();
                if (y < startY || y > endY) {
                    continue;
                }
                const float t = (y - startY) / (endY - startY);
                QVector<QVector3D> slice;
                for (int k = 0; k < loft.start.size(); ++k) {
                    slice.append(loft.start.at(k) + (loft.end.at(k) - loft.start.at(k)) * t);
                }
                if (const auto span = polygonSpanAt(slice, x + kSliceMeters / 2.0f)) {
                    spans.append(*span);
                }
            }
            std::sort(spans.begin(), spans.end());
            float covered = 0.0f;
            float reached = std::numeric_limits<float>::lowest();
            for (const auto& [low, high] : std::as_const(spans)) {
                covered += std::max(0.0f, high - std::max(low, reached));
                reached = std::max(reached, high);
            }
            expected += covered * kSliceMeters * kSliceMeters;
        }
    }

    const double volume = cwPassageTubeUnion::volume(debug.mesh);
    INFO("union volume " << volume << " m³, sliced " << expected << " m³");
    CHECK(volume == Catch::Approx(expected).epsilon(kVolumeTolerance));
}
