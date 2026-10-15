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
#include "cwPassageTubeJunction.h"
#include "cwPassageTubeMeshCheck.h"

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

// The ring nearest a station: its own ring, or the middle ring of the elbow
// that rounds a sharp corner there, which carries the station's readings.
const Builder::Ring& stationRing(const Builder::Chain& chain, const QVector3D& station)
{
    REQUIRE_FALSE(chain.rings.isEmpty());
    return *std::min_element(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& a, const Builder::Ring& b) {
        return a.center.distanceToPoint(station) < b.center.distanceToPoint(station);
    });
}

// The indices of the rings that turn from incoming to outgoing: those of the
// elbow rounding a corner between legs along them, and any rings inserted
// between its rings.
QVector<int> elbowRings(const Builder::Chain& chain, const QVector3D& incoming, const QVector3D& outgoing)
{
    constexpr float kAlongLegCosine = 0.9999f;
    QVector<int> elbow;
    for (int i = 0; i < chain.rings.size(); ++i) {
        const QVector3D& tangent = chain.rings.at(i).frame.tangent;
        if (QVector3D::dotProduct(tangent, incoming) < kAlongLegCosine && QVector3D::dotProduct(tangent, outgoing) < kAlongLegCosine) {
            elbow.append(i);
        }
    }
    return elbow;
}

// Whether every wall vertex of ring, before any pull, lies in front of the
// plane of the ring before it, so the tube between them never folds.
bool isInFrontOf(const Builder::Ring& ring, const Builder::Ring& previous)
{
    for (int bin = 0; bin < ring.radii.size(); ++bin) {
        const QVector3D vertex = ring.center + binDirection(ring, bin) * static_cast<float>(ring.radii.at(bin));
        if (QVector3D::dotProduct(vertex - previous.center, previous.frame.tangent) <= 0.0f) {
            return false;
        }
    }
    return true;
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

// Every ring vertex sits at its bin's radius: the bend rules kept each ring in
// front of the one before, so the fold-back pull never moved a vertex.
void checkNoVertexPulled(const Builder::Debug& debug)
{
    for (const Builder::Chain& chain : debug.chains) {
        for (const Builder::Ring& ring : chain.rings) {
            const int resolution = ring.radii.size();
            REQUIRE(ring.vertices.size() == resolution);
            for (int bin = 0; bin < resolution; ++bin) {
                const QVector3D unpulled = ring.center + binDirection(ring, bin) * static_cast<float>(ring.radii.at(bin));
                INFO("ring at (" << ring.center.x() << ", " << ring.center.y() << ", " << ring.center.z() << ") bin " << bin);
                CHECK(ring.vertices.at(bin).distanceToPoint(unpulled) < 1e-4f);
            }
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

// The point of the triangle abc closest to p (Ericson, Real-Time Collision
// Detection 5.1.5).
QVector3D closestPointOnTriangle(const QVector3D& p, const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    const QVector3D ab = b - a;
    const QVector3D ac = c - a;
    const QVector3D ap = p - a;
    const float d1 = QVector3D::dotProduct(ab, ap);
    const float d2 = QVector3D::dotProduct(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a;
    }
    const QVector3D bp = p - b;
    const float d3 = QVector3D::dotProduct(ab, bp);
    const float d4 = QVector3D::dotProduct(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        return a + ab * (d1 / (d1 - d3));
    }
    const QVector3D cp = p - c;
    const float d5 = QVector3D::dotProduct(ab, cp);
    const float d6 = QVector3D::dotProduct(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        return a + ac * (d2 / (d2 - d6));
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const float denominator = 1.0f / (va + vb + vc);
    return a + ab * (vb * denominator) + ac * (vc * denominator);
}

// The distance from point to the chain's side wall: the triangles between
// its consecutive rings, each quad split along its shorter diagonal as the
// mesh splits it, before any junction window is cut out of them.
double distanceToSides(const Builder::Chain& chain, const QVector3D& point)
{
    double nearest = std::numeric_limits<double>::max();
    const auto measure = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c) {
        nearest = std::min(nearest, static_cast<double>(closestPointOnTriangle(point, a, b, c).distanceToPoint(point)));
    };
    for (int i = 0; i + 1 < chain.rings.size(); ++i) {
        const QVector<QVector3D>& ring = chain.rings.at(i).vertices;
        const QVector<QVector3D>& next = chain.rings.at(i + 1).vertices;
        if (ring.isEmpty() || next.size() != ring.size()) {
            continue;
        }
        const int resolution = ring.size();
        for (int k = 0; k < resolution; ++k) {
            const int column = (k + 1) % resolution;
            const QVector3D& a0 = ring.at(k);
            const QVector3D& a1 = ring.at(column);
            const QVector3D& b0 = next.at(k);
            const QVector3D& b1 = next.at(column);
            if ((b1 - a0).lengthSquared() <= (b0 - a1).lengthSquared()) {
                measure(a0, b0, b1);
                measure(a0, b1, a1);
            } else {
                measure(a0, b0, a1);
                measure(b0, b1, a1);
            }
        }
    }
    return nearest;
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

QVector<QVector3D> mouthPositions(const QVector<cwPassageTubeBuilder::MouthPoint>& mouth)
{
    QVector<QVector3D> positions;
    positions.reserve(mouth.size());
    for (const cwPassageTubeBuilder::MouthPoint& point : mouth) {
        positions.append(point.position);
    }
    return positions;
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

// Three legs as big as each other meet a small one at j. The small leg is
// walked first, and the straightest leg as big as itself is n; but w and e
// are bigger than s and turn least into each other, so they are the through
// although n ties them.
TEST_CASE("cwPassageTubeBuilder chains the straightest pair of big legs when a third ties them", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList big = lrud(2, 2, 2, 2);
    const QStringList small = lrud(0.5, 0.5, 0.5, 0.5);
    const StationInput south {QStringLiteral("s"), QVector3D(0, -10, 0), small};
    const StationInput junctionSmall {QStringLiteral("j"), QVector3D(0, 0, 0), small};
    const StationInput junctionBig {QStringLiteral("j"), QVector3D(0, 0, 0), big};
    const StationInput west {QStringLiteral("w"), QVector3D(-10, 0, 0), big};
    const StationInput east {QStringLiteral("e"), QVector3D(10, 0, 0), big};
    const StationInput north {QStringLiteral("n"), QVector3D(0, 10, 0), big};
    const BuilderInput input = makeInput({{{south, junctionSmall}, {west, junctionBig, east}, {junctionBig, north}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 3);
    checkChainsCoverLegs(debug);
    const auto through = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [](const Builder::Chain& chain) {
        return chain.legs.size() == 2;
    });
    REQUIRE(through != debug.chains.cend());
    const QStringList stations = chainStations(debug, *through);
    CHECK((stations == QStringList({QStringLiteral("w"), QStringLiteral("j"), QStringLiteral("e")})
           || stations == QStringList({QStringLiteral("e"), QStringLiteral("j"), QStringLiteral("w")})));
}

// The big leg leaves j between the two small ones, which turn 90 degrees
// there, so neither continues it. It ends past j as the junction's through,
// by more than the small legs' largest reading, and both small legs open
// into its wall.
TEST_CASE("cwPassageTubeBuilder ends the biggest leg past a junction no other leg continues", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kSmallReading = 1.0;
    const QStringList small = lrud(kSmallReading, kSmallReading, kSmallReading, kSmallReading);
    const QStringList big = lrud(3, 3, 3, 3);
    const QVector3D junction(0, 0, 0);
    const QVector3D bigFar(-7.0f, -7.0f, 0.0f);
    const BuilderInput input = makeInput({{
        {{QStringLiteral("s"), QVector3D(0, -10, 0), small}, {QStringLiteral("j"), junction, small}, {QStringLiteral("w"), QVector3D(-10, 0, 0), small}},
        {{QStringLiteral("b"), bigFar, big}, {QStringLiteral("j"), junction, big}},
    }});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 3);
    checkChainsCoverLegs(debug);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::JunctionUnzipped) == 0);

    const auto bigChain = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [&](const Builder::Chain& chain) {
        return chainStations(debug, chain).contains(QStringLiteral("b"));
    });
    REQUIRE(bigChain != debug.chains.cend());
    REQUIRE(chainStations(debug, *bigChain) == QStringList({QStringLiteral("b"), QStringLiteral("j")}));
    CHECK(bigChain->endThroughReachMeters > kSmallReading);
    CHECK(bigChain->startThroughReachMeters == 0.0);
    CHECK(bigChain->endCap == Builder::EndCap::Fan);
    const QVector3D arriving = (junction - bigFar).normalized();
    CHECK(QVector3D::dotProduct(bigChain->rings.last().center - junction, arriving) > 0.0f);

    QSet<int> zipped;
    for (const Builder::JunctionWindow& window : bigChain->windows) {
        for (const Builder::WindowMouth& mouth : window.mouths) {
            zipped.insert(mouth.armChain);
        }
    }
    CHECK(zipped.size() == 2);
    checkMeshClosed(debug.mesh);
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

// How far the rings beside a corner sit from it on long legs: far enough
// that a ring of the passage's radius stays clear of the corner's ring.
double cornerInset(double radius, double turnDegrees)
{
    constexpr double kInsetSafety = 1.1;
    constexpr double kMinInsetMeters = 0.5;
    return std::max(kMinInsetMeters, kInsetSafety * radius * std::tan(qDegreesToRadians(turnDegrees) / 2.0));
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

TEST_CASE("cwPassageTubeBuilder keeps frames continuous through a vertical pit", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(verticalPitInput());
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    checkFramesOrthonormal(chain);
    checkMeshClosedWithSaneRings(debug);

    // The pit is round at its own left and right.
    const Builder::Ring& pit = ringAt(chain, QVector3D(0, 10, -5));
    CHECK(pit.frame.inclinationWeight == Catch::Approx(1.0));
    checkVectorNear(pit.frame.tangent, QVector3D(0, 0, -1));
    CHECK_FALSE(pit.measured.contains(false));
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

TEST_CASE("cwPassageTubeBuilder takes a wall at face value across the ring", "[PassageTube][cwPassageTubeBuilder]")
{
    // b faces the chord a→c, (1, 1, 0)/√2, which runs with the through d→b→c,
    // so b's 5 m left wall lies its full length to the through's left. b reads
    // taller on the chunk b→d than on a→b→c, so a→b→c, though wider, is no
    // big passage there; the chunk b→d comes first, and the walk from d
    // runs on along b→c, as big as a→b at b and straighter.
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
    const auto passesThroughB = [&](const Builder::Chain& chain) { return chain.legs.size() == 2; };
    const auto through = std::find_if(debug.chains.cbegin(), debug.chains.cend(), passesThroughB);
    const auto arm = std::find_if_not(debug.chains.cbegin(), debug.chains.cend(), passesThroughB);
    REQUIRE(through != debug.chains.cend());
    REQUIRE(arm != debug.chains.cend());
    CHECK(chainStations(debug, *through) == QStringList({QStringLiteral("d"), QStringLiteral("b"), QStringLiteral("c")}));
    CHECK(chainStations(debug, *arm) == QStringList({QStringLiteral("a"), QStringLiteral("b")}));

    const QVector3D bPosition = b.position.value();
    checkFarthestVertexNear(ringAt(*through, bPosition), bPosition + QVector3D(-kLeftWall, 0, 0), kToleranceMeters);

    // The arm arrives along +x into that wall and stops where it meets it,
    // on the through's triangles: between the grown rings at b they lie off
    // the ring tube's blend.
    constexpr double kOnMeshMeters = 1e-4;
    REQUIRE_FALSE(arm->endMouth.isEmpty());
    for (const QVector3D& point : mouthPositions(arm->endMouth)) {
        INFO("mouth point (" << point.x() << ", " << point.y() << ", " << point.z() << ")");
        CHECK(point.x() < bPosition.x());
        CHECK(distanceToSides(*through, point) < kOnMeshMeters);
    }
}

TEST_CASE("cwPassageTubeBuilder falls back to the next station's facing on a vertical shot", "[PassageTube][cwPassageTubeBuilder]")
{
    // b's chord in chunk 2 drops straight down, so b takes q's facing (the
    // chord b→r, west) and its 3 m left wall lies to the south of b. b's
    // ring is the middle ring of an elbow, 1.4 m below b on its tilted
    // plane, so it meets that wall between two bins, 2.49 m south of b, and
    // its 0 m right wall to the north on b.
    constexpr float kToleranceMeters = 0.2f;
    constexpr float kSouthReachMeters = 2.49f;
    constexpr float kReachToleranceMeters = 0.01f;
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
    const Builder::Ring& ring = stationRing(debug.chains.at(0), bPosition);
    const auto [southmost, northmost] = std::minmax_element(ring.vertices.cbegin(), ring.vertices.cend(), [](const QVector3D& a, const QVector3D& b) {
        return a.y() < b.y();
    });
    CHECK(southmost->y() == Catch::Approx(bPosition.y() - kSouthReachMeters).margin(kReachToleranceMeters));
    CHECK(northmost->y() < bPosition.y() + kToleranceMeters);
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

namespace {

// The unit normals of the triangles lying in the horizontal plane at height z.
QVector<QVector3D> normalsOfTrianglesAtHeight(const cwPassageTubeMesh& mesh, float z)
{
    constexpr float kOnPlaneMeters = 1e-4f;
    QVector<QVector3D> normals;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const QVector3D a = mesh.positions.at(mesh.indices.at(i));
        const QVector3D b = mesh.positions.at(mesh.indices.at(i + 1));
        const QVector3D c = mesh.positions.at(mesh.indices.at(i + 2));
        const bool isAtHeight = std::abs(a.z() - z) < kOnPlaneMeters
                                && std::abs(b.z() - z) < kOnPlaneMeters
                                && std::abs(c.z() - z) < kOnPlaneMeters;
        if (isAtHeight) {
            normals.append(QVector3D::crossProduct(b - a, c - a).normalized());
        }
    }
    return normals;
}

// A dead end closed flat: at least N - 2 triangles in the plane at height z,
// each facing within a degree of straight up or down, and no vertex past it.
void checkFlatCapAtHeight(const cwPassageTubeMesh& mesh, float z, float outward)
{
    const float minCosine = static_cast<float>(std::cos(qDegreesToRadians(1.0)));
    const QVector<QVector3D> normals = normalsOfTrianglesAtHeight(mesh, z);
    INFO("cap at z " << z);
    CHECK(normals.size() >= Builder::Options().ringResolution - 2);
    for (const QVector3D& normal : normals) {
        CHECK(QVector3D::dotProduct(normal, QVector3D(0, 0, outward)) > minCosine);
    }
    for (const QVector3D& position : mesh.positions) {
        CHECK(outward * (position.z() - z) < 1e-4f);
    }
}

} // namespace

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

    // Pit rings are round at their stations' left and right.
    CHECK_FALSE(chain.rings.at(5).measured.contains(false));
    for (double radius : chain.rings.at(5).radii) {
        CHECK(radius == Catch::Approx(1.0));
    }

    // Each end runs on to a flat ceiling or floor at the offset.
    checkFlatCapAtHeight(debug.mesh, 2.0f, 1.0f);
    checkFlatCapAtHeight(debug.mesh, -13.0f, -1.0f);
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder fits a pit from its own left and right", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kPitRadius = 1.5;
    const QStringList sides {QString::number(kPitRadius), QString::number(kPitRadius), QString(), QString()};
    const BuilderInput input = makeInput({{{
        {QStringLiteral("top"), QVector3D(0, 0, 0), sides},
        {QStringLiteral("bottom"), QVector3D(0, 0, -12), sides},
    }}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    for (const Builder::Ring& ring : debug.chains.at(0).rings) {
        INFO("ring at z " << ring.center.z());
        CHECK_FALSE(ring.measured.contains(false));
        for (double radius : ring.radii) {
            CHECK(radius == Catch::Approx(kPitRadius).margin(1e-3));
        }
    }
    CHECK(debug.warnings.isEmpty());

    checkFlatCapAtHeight(debug.mesh, 0.0f, 1.0f);
    const QVector<cwPassageTubeMeshCheck::Issue> missed = cwPassageTubeMeshCheck::wallsOnSurface(debug);
    INFO(cwPassageTubeMeshCheck::toText({missed, 0, {}}).toStdString());
    CHECK(missed.isEmpty());
    checkMeshClean(debug);
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
    // The lip and the bottom turn 90°, so their elbows' middle rings lie at
    // 45°, off their stations toward the bend's inside, and reach the walls
    // of the two legs: the level leg's ceiling or floor √2 times its reading
    // from the station outside the bend, the shaft's wall inside it, where
    // the level leg's floor or ceiling runs on into the shaft.
    constexpr double kShaftRadius = 2.0;
    const QVector3D lipStation(0, 10, 0);
    const Builder::Ring& lip = stationRing(chain, lipStation);
    const double lipOffset = QVector3D::dotProduct(lipStation - lip.center, lip.frame.up);
    REQUIRE(lipOffset > 0.0);
    CHECK(lip.radii.at(binAt(90)) == Catch::Approx(1.0 * std::numbers::sqrt2 + lipOffset));
    CHECK(lip.radii.at(binAt(270)) == Catch::Approx((lip.center.y() - (lipStation.y() - kShaftRadius)) * std::numbers::sqrt2));
    // The corner inset is 1.1 × the 2 m walls × tan 45°.
    const Builder::Ring& approach = ringAt(chain, QVector3D(0, 7.8f, 0));
    CHECK(approach.radii.at(binAt(270)) == Catch::Approx(0.5));

    const QVector3D bottomStation(0, 10, -20);
    const Builder::Ring& bottom = stationRing(chain, bottomStation);
    const double bottomOffset = QVector3D::dotProduct(bottomStation - bottom.center, -bottom.frame.up);
    REQUIRE(bottomOffset > 0.0);
    CHECK(bottom.radii.at(binAt(90)) == Catch::Approx((bottomStation.y() + kShaftRadius - bottom.center.y()) * std::numbers::sqrt2));
    CHECK(bottom.radii.at(binAt(270)) == Catch::Approx(0.5 * std::numbers::sqrt2 + bottomOffset));

    // The shaft is round at its stations' left and right.
    const Builder::Ring& shaft = ringAt(chain, QVector3D(0, 10, -10));
    CHECK_FALSE(shaft.measured.contains(false));
    for (double radius : shaft.radii) {
        CHECK(radius == Catch::Approx(2.0));
    }
    for (const Builder::Ring& ring : chain.rings) {
        for (double radius : ring.radii) {
            CHECK(radius <= 2.0 + lipOffset + 1e-6);
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

namespace {

constexpr double kSlopeDegrees = 30.0;
constexpr double kSlopedLegMeters = 10.0;

// A straight passage climbing at 30° from a through b to c, every station
// reading the given ceiling and a 1 m floor and walls.
BuilderInput slopedPassageInput(double up)
{
    const double slope = qDegreesToRadians(kSlopeDegrees);
    const QVector3D leg = QVector3D(static_cast<float>(std::cos(slope)), 0.0f, static_cast<float>(std::sin(slope)))
                          * static_cast<float>(kSlopedLegMeters);
    const QStringList dimensions = lrud(1, 1, up, 1);
    return makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), leg, dimensions},
        {QStringLiteral("c"), leg * 2.0f, dimensions},
    }}});
}

} // namespace

TEST_CASE("cwPassageTubeBuilder foreshortens up and down on a slope", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kUp = 1.5;
    const Builder::Debug debug = buildDebug(slopedPassageInput(kUp));
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    REQUIRE(chain.rings.size() > 2);

    const double across = kUp * std::cos(qDegreesToRadians(kSlopeDegrees));
    for (int i = 1; i + 1 < chain.rings.size(); ++i) {
        const Builder::Ring& ring = chain.rings.at(i);
        INFO("ring " << i);
        CHECK(ring.radii.at(binAt(90)) == Catch::Approx(across).margin(1e-3));
        checkVectorNear(binDirection(ring, binAt(90)), ring.frame.up);
    }

    const QVector<cwPassageTubeMeshCheck::Issue> missed = cwPassageTubeMeshCheck::wallsOnSurface(debug);
    INFO(cwPassageTubeMeshCheck::toText({missed, 0, {}}).toStdString());
    CHECK(missed.isEmpty());
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder ends a sloped passage with a plumb ring", "[PassageTube][cwPassageTubeBuilder]")
{
    const auto bendClampedCount = [](const Builder::Debug& debug) {
        return std::count_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
            return warning.type == cwPassageTubeMesh::Warning::BendClamped;
        });
    };

    SECTION("The end rings stand in the vertical plane through their stations")
    {
        constexpr double kUp = 1.5;
        const Builder::Debug debug = buildDebug(slopedPassageInput(kUp));
        REQUIRE(debug.chains.size() == 1);
        const Builder::Chain& chain = debug.chains.at(0);
        const QVector<QVector3D> stations {debug.legs.first().fromPosition, debug.legs.last().toPosition};
        const QVector<const Builder::Ring*> ends {&chain.rings.first(), &chain.rings.last()};
        for (int end = 0; end < ends.size(); ++end) {
            const Builder::Ring& ring = *ends.at(end);
            INFO("end " << end);
            checkVectorNear(ring.center, stations.at(end));
            CHECK(std::abs(ring.frame.tangent.z()) < 1e-6f);
            checkVectorNear(ring.frame.up, QVector3D(0, 0, 1));
            checkVectorNear(ring.vertices.at(binAt(90)), stations.at(end) + QVector3D(0, 0, static_cast<float>(kUp)));
        }
        // A 1.5 m ceiling folds nowhere near the ring 1 m in, so no inset.
        CHECK(chain.rings.at(1).center.distanceToPoint(chain.rings.first().center) == Catch::Approx(1.0).margin(1e-3));
        CHECK(bendClampedCount(debug) == 0);
        checkMeshClean(debug);
    }

    SECTION("A tall section insets the ring beside a plumb end")
    {
        // The start climbs, so its ring folds toward the ceiling: the ring
        // square to the leg sits 1.1 · 7 · tan 30° along it.
        constexpr double kUp = 7.0;
        const double inset = 1.1 * kUp * std::tan(qDegreesToRadians(kSlopeDegrees));
        const Builder::Debug debug = buildDebug(slopedPassageInput(kUp));
        REQUIRE(debug.chains.size() == 1);
        const Builder::Chain& chain = debug.chains.at(0);
        CHECK(std::abs(chain.rings.first().frame.tangent.z()) < 1e-6f);
        CHECK(chain.rings.at(1).center.distanceToPoint(chain.rings.first().center) == Catch::Approx(inset).margin(1e-3));
        CHECK(bendClampedCount(debug) == 0);
        checkMeshClean(debug);
        checkNoVertexPulled(debug);
    }
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
    // Two rings of vertices, each closed flat across itself.
    CHECK(mesh.positions.size() == 2 * resolution);
    // Two side triangles per bin plus N - 2 cap triangles at each end.
    CHECK(mesh.indices.size() == 3 * (2 * resolution + 2 * (resolution - 2)));
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

TEST_CASE("cwPassageTubeBuilder rounds a 95 degree bend with inset rings", "[PassageTube][cwPassageTubeBuilder]")
{
    const QVector3D& corner = kBendCorner;
    const QVector3D& outgoing = kSharpestBendOutgoing;
    const Builder::Debug debug = buildDebug(sharpestBendInput());
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const float inset = static_cast<float>(cornerInset(1.0, kSharpestBendDegrees));
    const Builder::Ring& before = ringAt(chain, corner - QVector3D(inset, 0, 0));
    checkVectorNear(before.frame.tangent, QVector3D(1, 0, 0));
    const Builder::Ring& after = ringAt(chain, corner + outgoing * inset);
    checkVectorNear(after.frame.tangent, outgoing);
    checkFramesOrthonormal(chain);
    checkMeshClosedWithSaneRings(debug);
}

TEST_CASE("cwPassageTubeBuilder rounds a sharp bend with elbow rings", "[PassageTube][cwPassageTubeBuilder]")
{
    // b has no readings, so its corner insets by the default radius and the
    // 1 m section borrowed from a and c reaches past the elbow's arc; the
    // middle ring, stretched across the bisector plane, is capped on the
    // inside at 0.95 of the arc's radius inset / tan 45°.
    constexpr double kConcavityClampFactor = 0.95;
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), kBendCorner},
        {QStringLiteral("c"), kBendCorner + QVector3D(0, 10, 0), dimensions},
    }}});
    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const QVector3D incoming(1, 0, 0);
    const QVector3D outgoing(0, 1, 0);
    const QVector<int> elbow = elbowRings(chain, incoming, outgoing);
    REQUIRE(elbow.size() == 3);
    for (int i = 1; i < chain.rings.size(); ++i) {
        INFO("ring " << i);
        CHECK(isInFrontOf(chain.rings.at(i), chain.rings.at(i - 1)));
    }

    const Builder::Ring& middle = chain.rings.at(elbow.at(1));
    checkVectorNear(middle.frame.tangent, (incoming + outgoing).normalized());
    const double arcRadius = cornerInset(Builder::Options().defaultRadiusMeters, 90.0);
    const QVector3D inside = (outgoing - incoming).normalized();
    const int innerBin = binAt(180);
    REQUIRE(QVector3D::dotProduct(binDirection(middle, innerBin), inside) == Catch::Approx(1.0).margin(1e-4));
    CHECK(middle.radii.at(innerBin) == Catch::Approx(kConcavityClampFactor * arcRadius).margin(1e-3));
    checkMeshClean(debug);
    checkNoVertexPulled(debug);
}

namespace {

constexpr float kParallelEpsilon = 1e-9f;
constexpr float kMinElbowOffsetMeters = 0.1f;

// The ring's radius toward direction, a unit vector in its plane: where the
// ray from its center meets the chord between the bins either side.
double radiusToward(const Builder::Ring& ring, const QVector3D& direction)
{
    const QVector2D ray = QVector2D(QVector3D::dotProduct(direction, ring.frame.right), QVector3D::dotProduct(direction, ring.frame.up)).normalized();
    const auto local = [&](int k) {
        return QVector2D(static_cast<float>(ring.radii.at(k) * std::cos(ring.binAngles.at(k))),
                         static_cast<float>(ring.radii.at(k) * std::sin(ring.binAngles.at(k))));
    };
    const int resolution = ring.radii.size();
    double nearest = std::numeric_limits<double>::max();
    for (int bin = 0; bin < resolution; ++bin) {
        const QVector2D a = local(bin);
        const QVector2D edge = local((bin + 1) % resolution) - a;
        const float denominator = ray.x() * edge.y() - ray.y() * edge.x();
        if (std::abs(denominator) < kParallelEpsilon) {
            continue;
        }
        const float along = (a.x() * edge.y() - a.y() * edge.x()) / denominator;
        const float onEdge = (a.x() * ray.y() - a.y() * ray.x()) / denominator;
        if (along > 0.0f && onEdge >= 0.0f && onEdge <= 1.0f) {
            nearest = std::min(nearest, static_cast<double>(along));
        }
    }
    return nearest;
}

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

TEST_CASE("cwPassageTubeBuilder keeps a corner station inside its elbow ring", "[PassageTube][cwPassageTubeBuilder]")
{
    const auto [name, corner] = GENERATE(table<QString, QString>({
        {QStringLiteral("23. Sharp bend at a station reading zero on the outside wall"), QStringLiteral("c1")},
        {QStringLiteral("24. Sharp bend in an unread passage next to a zero reading"), QStringLiteral("u2")},
    }));
    INFO(name.toStdString());
    const cwPassageTubeCases::Case& entry = catalogCase(name);
    const Builder::Debug debug = buildCase(entry);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const QVector3D station = casePosition(entry, corner);
    const Builder::Ring& middle = stationRing(chain, station);
    REQUIRE(middle.isElbow);
    const QVector3D offset = station - middle.center;
    REQUIRE(std::abs(QVector3D::dotProduct(offset, middle.frame.tangent)) < 1e-4f);
    REQUIRE(offset.length() > kMinElbowOffsetMeters);
    CHECK(radiusToward(middle, offset.normalized()) > offset.length());
}

// c1 reads 0 on its left, the bend's outside. The walls on the outside stay
// on the tube; the right wall lies on the inside, where the elbow reads it
// from its middle ring's center.
TEST_CASE("cwPassageTubeBuilder keeps a corner station's outside walls on its elbow", "[PassageTube][cwPassageTubeBuilder]")
{
    const cwPassageTubeCases::Case& entry = catalogCase(QStringLiteral("23. Sharp bend at a station reading zero on the outside wall"));
    const Builder::Debug debug = buildCase(entry);
    REQUIRE(debug.chains.size() == 1);

    const QVector3D corner = casePosition(entry, QStringLiteral("c1"));
    const QVector3D incoming = (corner - casePosition(entry, QStringLiteral("c0"))).normalized();
    const QVector3D outgoing = (casePosition(entry, QStringLiteral("c2")) - corner).normalized();
    const QVector3D inside = (outgoing - incoming).normalized();

    const auto leg = std::find_if(debug.legs.cbegin(), debug.legs.cend(), [](const Builder::Leg& candidate) {
        return candidate.to == QStringLiteral("c1");
    });
    REQUIRE(leg != debug.legs.cend());
    const QVector3D right(leg->toFacing.y(), -leg->toFacing.x(), 0.0f);
    const QVector3D up(0.0f, 0.0f, 1.0f);
    const cwStationLrud& lrud = leg->toPassageLrud;
    const QList<std::pair<std::optional<double>, QVector3D>> walls {{lrud.left, -right}, {lrud.right, right}, {lrud.up, up}, {lrud.down, -up}};
    int outsideWalls = 0;
    for (const auto& [reading, direction] : walls) {
        if (!reading || QVector3D::dotProduct(direction, inside) >= 0.0f) {
            continue;
        }
        ++outsideWalls;
        const QVector3D wall = corner + direction * static_cast<float>(*reading);
        INFO("wall (" << wall.x() << ", " << wall.y() << ", " << wall.z() << ")");
        CHECK(distanceToSides(debug.chains.at(0), wall) <= cwPassageTubeMeshCheck::kWallTolerance);
    }
    CHECK(outsideWalls == 2);
    checkMeshClean(debug);
}

// c1's elbow carries its readings off the station: the middle ring's section
// sits around c1 and the ring keeps within it, seen from the ring's center.
// A ring the bend left measured is its own section.
TEST_CASE("cwPassageTubeBuilder keeps the section each ring was fitted to", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr float kStationTolerance = 1e-4f;
    constexpr double kOnSectionMeters = 1e-4;
    const cwPassageTubeCases::Case& entry = catalogCase(QStringLiteral("23. Sharp bend at a station reading zero on the outside wall"));
    const Builder::Debug debug = buildCase(entry);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const QVector3D corner = casePosition(entry, QStringLiteral("c1"));
    const Builder::Ring& middle = stationRing(chain, corner);
    REQUIRE(middle.isElbow);
    REQUIRE(middle.section);
    CHECK(middle.section->station.distanceToPoint(corner) < kStationTolerance);
    CHECK(middle.section->readings.left);
    CHECK(middle.section->readings.right);
    CHECK(middle.section->readings.up);
    CHECK(middle.section->readings.down);

    REQUIRE(middle.section->stationRadii.size() == middle.radii.size());
    CHECK(middle.center.distanceToPoint(corner) > kMinElbowOffsetMeters);
    const QVector<std::optional<cwPassageTubeMeshCheck::WallReach>> reaches = cwPassageTubeMeshCheck::wallReaches(debug, chain, middle);
    REQUIRE(reaches.size() == middle.vertices.size());
    for (int bin = 0; bin < reaches.size(); ++bin) {
        INFO("bin " << bin);
        REQUIRE(reaches.at(bin));
        CHECK(reaches.at(bin)->beyondMeters <= kOnSectionMeters);
    }

    for (const Builder::Ring& ring : chain.rings) {
        if (ring.isElbow && &ring != &middle) {
            CHECK_FALSE(ring.section);
        }
    }

    const auto straight = std::find_if(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& ring) {
        return !ring.isElbow && std::all_of(ring.measured.cbegin(), ring.measured.cend(), [](bool measured) { return measured; });
    });
    REQUIRE(straight != chain.rings.cend());
    REQUIRE(straight->section);
    CHECK(straight->section->station.distanceToPoint(straight->center) < kStationTolerance);
    CHECK(straight->section->stationRadii == straight->radii);
}

// b turns 90° left with its 3 m left wall on the bend's inside, as Phake Cave
// a3 turns with its right. The elbow's middle ring sits off b toward the
// inside and takes b's section seen from its center, so its inside wall lands
// where b's left wall slides along a leg onto the ring's 45° plane, √2 times
// the reading from b, instead of that far from the ring's center.
TEST_CASE("cwPassageTubeBuilder keeps a sharp bend's rings within the station's readings", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kInsideWall = 3.0;
    constexpr float kOnWallMeters = 1e-3f;
    const QVector3D corner(10, 0, 0);
    const Builder::Debug debug = buildDebug(makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), lrud(1.5, 1.5, 4.5, 1)},
        {QStringLiteral("b"), corner, lrud(kInsideWall, 1.5, 4.5, 1)},
        {QStringLiteral("c"), QVector3D(10, 10, 0), lrud(1.5, 1.5, 4.5, 1)},
    }}}));
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const Builder::Ring& middle = stationRing(chain, corner);
    REQUIRE(middle.isElbow);
    REQUIRE(middle.center.distanceToPoint(corner) > kMinElbowOffsetMeters);
    const QVector3D insideWall = middle.vertices.at(binAt(180));
    CHECK(insideWall.distanceToPoint(corner) <= static_cast<float>(kInsideWall * std::numbers::sqrt2) + kOnWallMeters);

    const QVector<cwPassageTubeMeshCheck::Issue> issues = cwPassageTubeMeshCheck::ringIssues(debug);
    INFO(cwPassageTubeMeshCheck::toText({issues, 0, {}}).toStdString());
    CHECK(std::none_of(issues.cbegin(), issues.cend(), [](const cwPassageTubeMeshCheck::Issue& issue) {
        return issue.type == cwPassageTubeMeshCheck::WallBeyondReading;
    }));
    checkMeshClean(debug);
}

// c0 is a dead end on a leg climbing 45°, so its plumb ring is the corner of
// a 90° bend whose inside is up. The ring's ceiling takes the miter of the
// capped ring before it only as far as c0's readings reach.
TEST_CASE("cwPassageTubeBuilder keeps a plumb end ring within the station's readings", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kRadiusTolerance = 1e-9;
    const cwPassageTubeCases::Case& entry = catalogCase(QStringLiteral("23. Sharp bend at a station reading zero on the outside wall"));
    const Builder::Debug debug = buildCase(entry);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Ring& end = stationRing(debug.chains.at(0), casePosition(entry, QStringLiteral("c0")));
    REQUIRE(end.section);
    for (int bin = 0; bin < end.radii.size(); ++bin) {
        INFO("bin " << bin);
        CHECK(end.radii.at(bin) <= end.section->stationRadii.at(bin) + kRadiusTolerance);
    }
}

// b turns left with its left wall, the bend's inside, unread: the elbow moves
// no measured wall, though its middle ring sits off b.
TEST_CASE("cwPassageTubeBuilder warns of no clamp at an elbow that keeps its measured walls", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kRadius = 1.0;
    const QStringList dimensions = lrud(kRadius, kRadius, kRadius, kRadius);
    const QStringList insideUnread {QString(), dimensions.at(1), dimensions.at(2), dimensions.at(3)};
    const Builder::Debug debug = buildDebug(makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), QVector3D(10, 0, 0), insideUnread},
        {QStringLiteral("c"), QVector3D(10, 10, 0), dimensions},
    }}}));
    REQUIRE(debug.chains.size() == 1);
    const Builder::Ring& middle = stationRing(debug.chains.at(0), QVector3D(10, 0, 0));
    REQUIRE(middle.isElbow);
    REQUIRE(middle.center.distanceToPoint(QVector3D(10, 0, 0)) > kMinElbowOffsetMeters);

    CHECK(std::none_of(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == cwPassageTubeMesh::Warning::BendClamped;
    }));
    checkMeshClean(debug);
}

namespace {

constexpr float kSameStationMeters = 1e-4f;

// Where a ray from a ring's center meets its wall, and whether both bins
// either side of the hit were measured.
struct WallHit {
    double reach = 0.0;
    bool isMeasured = false;
};

// How far the ring's wall lies from its center along ray, in the ring's
// (right, up) plane: where the ray meets the chord between the bins either
// side.
std::optional<WallHit> wallHitAlong(const Builder::Ring& ring, const QVector2D& ray)
{
    const auto local = [&](int k) {
        return QVector2D(static_cast<float>(ring.radii.at(k) * std::cos(ring.binAngles.at(k))),
                         static_cast<float>(ring.radii.at(k) * std::sin(ring.binAngles.at(k))));
    };
    const auto cross = [](const QVector2D& a, const QVector2D& b) {
        return a.x() * b.y() - a.y() * b.x();
    };
    const int resolution = ring.radii.size();
    std::optional<WallHit> nearest;
    for (int bin = 0; bin < resolution; ++bin) {
        const int next = (bin + 1) % resolution;
        const QVector2D a = local(bin);
        const QVector2D edge = local(next) - local(bin);
        const float denominator = cross(ray, edge);
        if (std::abs(denominator) < kParallelEpsilon) {
            continue;
        }
        const float along = cross(a, edge) / denominator;
        const float onEdge = cross(a, ray) / denominator;
        if (along > 0.0f && onEdge >= 0.0f && onEdge <= 1.0f && (!nearest || along < nearest->reach)) {
            nearest = WallHit {along, ring.measured.at(bin) && ring.measured.at(next)};
        }
    }
    return nearest;
}

// The ring's wall along ray, unless a sharp bend reshaped it, leaving either
// bin unmeasured.
std::optional<double> measuredReachAlong(const Builder::Ring& ring, const QVector2D& ray)
{
    const std::optional<WallHit> hit = wallHitAlong(ring, ray);
    if (hit && hit->isMeasured) {
        return hit->reach;
    }
    return std::nullopt;
}

std::optional<double> lerpReading(const std::optional<double>& a, const std::optional<double>& b, double t)
{
    if (a && b) {
        return *a + (*b - *a) * t;
    }
    return std::nullopt;
}

// A station's record across a ring at face value: left and right as read,
// swapped when the record faces against the ring's tangent; up and down
// plumb, so a ring square to a sloped leg meets them foreshortened.
cwStationLrud recordAcross(const cwStationLrud& lrud, const QVector3D& facing, const Builder::Frame& frame)
{
    const QVector3D worldUp(0.0f, 0.0f, 1.0f);
    const double plumb = std::abs(QVector3D::dotProduct(worldUp, frame.up));
    const auto foreshortened = [&](const std::optional<double>& reading) -> std::optional<double> {
        if (reading) {
            return std::max(Builder::kMinRadiusMeters, *reading * plumb);
        }
        return std::nullopt;
    };
    const bool facesAgainst = QVector3D::dotProduct(facing, frame.tangent) < 0.0f;
    return {facesAgainst ? lrud.right : lrud.left,
            facesAgainst ? lrud.left : lrud.right,
            foreshortened(lrud.up),
            foreshortened(lrud.down)};
}

// The record a leg holds for its station at position, as the leg's chunk
// read it.
std::optional<std::pair<cwStationLrud, QVector3D>> legRecordAt(const Builder::Leg& leg, const QVector3D& position)
{
    if (leg.fromPosition.distanceToPoint(position) < kSameStationMeters) {
        return std::pair {leg.fromPassageLrud, leg.fromFacing};
    }
    if (leg.toPosition.distanceToPoint(position) < kSameStationMeters) {
        return std::pair {leg.toPassageLrud, leg.toFacing};
    }
    return std::nullopt;
}

// The widest of the station's records on the chain's legs, across the ring
// at the station.
cwStationLrud stationReadingsAcross(const Builder::Debug& debug, const Builder::Chain& chain, const QVector3D& station, const Builder::Frame& frame)
{
    const auto widen = [](std::optional<double>& widest, const std::optional<double>& reading) {
        if (reading) {
            widest = std::max(widest.value_or(*reading), *reading);
        }
    };
    cwStationLrud widest;
    for (const Builder::ChainLeg& chainLeg : chain.legs) {
        if (const auto record = legRecordAt(debug.legs.at(chainLeg.legIndex), station)) {
            const cwStationLrud across = recordAcross(record->first, record->second, frame);
            widen(widest.left, across.left);
            widen(widest.right, across.right);
            widen(widest.up, across.up);
            widen(widest.down, across.down);
        }
    }
    return widest;
}

// The walls of one ring along ±right and ±up from its center and the
// readings they should reach.
struct RingWalls {
    int ring = -1;
    cwStationLrud expected;
    cwStationLrud reached;
};

// A ring along a leg reaches its two stations' readings interpolated by its
// distance along the leg; a ring at a station reaches the station's
// readings. Elbow rings, which reach the walls of the corner's two legs, and
// rings without readings of their own (borrowed, steep and grown rings)
// carry none.
std::optional<RingWalls> ringWalls(const Builder::Debug& debug, const Builder::Chain& chain, int ringIndex)
{
    const Builder::Ring& ring = chain.rings.at(ringIndex);
    if (!ring.section || ring.isElbow || ring.isGrown || ring.frame.inclinationWeight > 0.0) {
        return std::nullopt;
    }

    RingWalls walls {ringIndex, {}, {}};
    walls.reached = {measuredReachAlong(ring, QVector2D(-1.0f, 0.0f)),
                     measuredReachAlong(ring, QVector2D(1.0f, 0.0f)),
                     measuredReachAlong(ring, QVector2D(0.0f, 1.0f)),
                     measuredReachAlong(ring, QVector2D(0.0f, -1.0f))};

    const Builder::ChainLeg& chainLeg = chain.legs.at(ring.chainLeg);
    const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
    const QVector3D start = chainLeg.reversed ? leg.toPosition : leg.fromPosition;
    const QVector3D end = chainLeg.reversed ? leg.fromPosition : leg.toPosition;
    if (ring.center.distanceToPoint(start) < kSameStationMeters || ring.center.distanceToPoint(end) < kSameStationMeters) {
        walls.expected = stationReadingsAcross(debug, chain, ring.center, ring.frame);
        return walls;
    }

    const QVector3D along = end - start;
    const double t = std::clamp(static_cast<double>(QVector3D::dotProduct(ring.center - start, along) / along.lengthSquared()), 0.0, 1.0);
    const auto startRecord = legRecordAt(leg, start);
    const auto endRecord = legRecordAt(leg, end);
    REQUIRE(startRecord);
    REQUIRE(endRecord);
    const cwStationLrud low = recordAcross(startRecord->first, startRecord->second, ring.frame);
    const cwStationLrud high = recordAcross(endRecord->first, endRecord->second, ring.frame);
    walls.expected = {lerpReading(low.left, high.left, t),
                      lerpReading(low.right, high.right, t),
                      lerpReading(low.up, high.up, t),
                      lerpReading(low.down, high.down, t)};
    return walls;
}

void checkWallsWithinReadings(const RingWalls& walls)
{
    INFO("ring " << walls.ring);
    const auto check = [](const char* name, const std::optional<double>& expected, const std::optional<double>& reached) {
        if (expected && reached) {
            INFO(name << " wall reaches " << *reached << ", readings " << *expected);
            CHECK(std::abs(*reached - *expected) <= cwPassageTubeMeshCheck::kWallTolerance);
        }
    };
    check("left", walls.expected.left, walls.reached.left);
    check("right", walls.expected.right, walls.reached.right);
    check("up", walls.expected.up, walls.reached.up);
    check("down", walls.expected.down, walls.reached.down);
}

// The chain holding the leg between the two named stations.
const Builder::Chain& chainWithLeg(const Builder::Debug& debug, const QString& from, const QString& to)
{
    const auto it = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [&](const Builder::Chain& chain) {
        return std::any_of(chain.legs.cbegin(), chain.legs.cend(), [&](const Builder::ChainLeg& chainLeg) {
            const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
            return leg.from == from && leg.to == to;
        });
    });
    REQUIRE(it != debug.chains.cend());
    return *it;
}

// Whether the ring lies on a leg between two of the named stations.
bool isOnLegBetween(const Builder::Debug& debug, const Builder::Chain& chain, const Builder::Ring& ring, const QStringList& stations)
{
    const Builder::Leg& leg = debug.legs.at(chain.legs.at(ring.chainLeg).legIndex);
    return stations.contains(leg.from) && stations.contains(leg.to);
}

} // namespace

// Between two stations each wall runs from the first station's reading to
// the second's by distance along the leg, on the legs of an S-bend and of
// Phake Cave's a1 → a4 chain, whose a2 corner the tube used to bulge around
// and neck down past. The corners' elbow rings are checked against the legs'
// walls on their own.
TEST_CASE("cwPassageTubeBuilder runs each wall from one station's reading to the next", "[PassageTube][cwPassageTubeBuilder]")
{
    SECTION("S-bend")
    {
        const cwPassageTubeCases::Case& entry = catalogCase(QStringLiteral("30. S-bend from a small to a large to a medium section"));
        const Builder::Debug debug = buildCase(entry);
        REQUIRE(debug.chains.size() == 1);
        const Builder::Chain& chain = debug.chains.at(0);
        int checkedRings = 0;
        for (int i = 0; i < chain.rings.size(); ++i) {
            if (const std::optional<RingWalls> walls = ringWalls(debug, chain, i)) {
                checkWallsWithinReadings(*walls);
                ++checkedRings;
            }
        }
        CHECK(checkedRings > chain.legs.size());
    }

    SECTION("Phake Cave a1 to a4")
    {
        constexpr double kA2Left = 2.5;
        constexpr double kA3Left = 1.5;
        constexpr double kMonotonicTolerance = 1e-3;
        const Builder::Debug debug = buildDebug(makeInput(cwPassageTubeCases::phakeCave()));
        const Builder::Chain& chain = chainWithLeg(debug, QStringLiteral("a1"), QStringLiteral("a2"));
        const QStringList stations {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3"), QStringLiteral("a4")};
        const auto a2Leg = std::find_if(debug.legs.cbegin(), debug.legs.cend(), [](const Builder::Leg& leg) {
            return leg.to == QStringLiteral("a2");
        });
        REQUIRE(a2Leg != debug.legs.cend());
        const QVector3D a2 = a2Leg->toPosition;

        int checkedRings = 0;
        QVector<std::pair<double, double>> a2ToA3Left;
        for (int i = 0; i < chain.rings.size(); ++i) {
            const Builder::Ring& ring = chain.rings.at(i);
            if (!isOnLegBetween(debug, chain, ring, stations)) {
                continue;
            }
            const std::optional<RingWalls> walls = ringWalls(debug, chain, i);
            if (!walls) {
                continue;
            }
            checkWallsWithinReadings(*walls);
            ++checkedRings;
            const bool isOnA2ToA3 = isOnLegBetween(debug, chain, ring, {QStringLiteral("a2"), QStringLiteral("a3")});
            if (isOnA2ToA3 && walls->reached.left) {
                a2ToA3Left.append({ring.section->station.distanceToPoint(a2), *walls->reached.left});
            }
        }
        CHECK(checkedRings > chain.legs.size());

        std::sort(a2ToA3Left.begin(), a2ToA3Left.end());
        REQUIRE(a2ToA3Left.size() > 2);
        QStringList leftText;
        for (const auto& [distance, left] : std::as_const(a2ToA3Left)) {
            leftText.append(QStringLiteral("%1 m: %2").arg(distance, 0, 'f', 2).arg(left, 0, 'f', 2));
        }
        INFO("a2 to a3 left walls " << leftText.join(QStringLiteral(", ")).toStdString());
        CHECK(a2ToA3Left.first().second <= kA2Left + cwPassageTubeMeshCheck::kWallTolerance);
        CHECK(a2ToA3Left.last().second >= kA3Left - cwPassageTubeMeshCheck::kWallTolerance);
        for (int i = 1; i < a2ToA3Left.size(); ++i) {
            INFO("ring " << i);
            CHECK(a2ToA3Left.at(i).second <= a2ToA3Left.at(i - 1).second + kMonotonicTolerance);
        }
    }
}

namespace {

// A station the chain walks through, in walking order.
struct ChainStation {
    QString name;
    QVector3D position;
};

QVector<ChainStation> chainStationPoints(const Builder::Debug& debug, const Builder::Chain& chain)
{
    QVector<ChainStation> stations;
    for (const Builder::ChainLeg& chainLeg : chain.legs) {
        const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
        if (stations.isEmpty()) {
            stations.append(chainLeg.reversed ? ChainStation {leg.to, leg.toPosition} : ChainStation {leg.from, leg.fromPosition});
        }
        stations.append(chainLeg.reversed ? ChainStation {leg.from, leg.fromPosition} : ChainStation {leg.to, leg.toPosition});
    }
    return stations;
}

// A corner station of the chain and the legs into and out of it, as indices
// into Chain::legs.
struct Corner {
    QString name;
    QVector3D station;
    int incomingLeg = -1;
    int outgoingLeg = -1;
    QVector3D incoming;
    QVector3D outgoing;

    // Horizontal, toward the inside of the bend.
    QVector3D inside() const
    {
        QVector3D toward = outgoing - incoming;
        toward.setZ(0.0f);
        return toward.normalized();
    }
};

// The interior station of the chain nearest point.
Corner nearestCorner(const Builder::Debug& debug, const Builder::Chain& chain, const QVector3D& point)
{
    const QVector<ChainStation> stations = chainStationPoints(debug, chain);
    REQUIRE(stations.size() > 2);
    int nearest = 1;
    for (int i = 2; i + 1 < stations.size(); ++i) {
        if (stations.at(i).position.distanceToPoint(point) < stations.at(nearest).position.distanceToPoint(point)) {
            nearest = i;
        }
    }
    const QVector3D& previous = stations.at(nearest - 1).position;
    const QVector3D& station = stations.at(nearest).position;
    const QVector3D& next = stations.at(nearest + 1).position;
    return {stations.at(nearest).name, station, nearest - 1, nearest, (station - previous).normalized(), (next - station).normalized()};
}

// A point's offset from a chain leg's axis, extended both ways without
// limit, and the leg's wall in that direction: the quadrant ellipse of its
// two stations' readings interpolated by the point's distance along the leg,
// holding either station's reading beyond it.
struct LegWall {
    double offset = 0.0;
    std::optional<double> wall;
};

LegWall legWallAt(const Builder::Debug& debug, const Builder::Chain& chain, int chainLegIndex, const QVector3D& point)
{
    const Builder::ChainLeg& chainLeg = chain.legs.at(chainLegIndex);
    const Builder::Leg& leg = debug.legs.at(chainLeg.legIndex);
    const QVector3D start = chainLeg.reversed ? leg.toPosition : leg.fromPosition;
    const QVector3D end = chainLeg.reversed ? leg.fromPosition : leg.toPosition;
    const QVector3D along = end - start;
    const float alongPoint = QVector3D::dotProduct(point - start, along) / along.lengthSquared();
    const QVector3D offset = point - (start + along * alongPoint);

    const QVector3D worldUp(0.0f, 0.0f, 1.0f);
    Builder::Frame frame;
    frame.tangent = along.normalized();
    frame.up = (worldUp - frame.tangent * QVector3D::dotProduct(worldUp, frame.tangent)).normalized();
    frame.right = QVector3D::crossProduct(frame.tangent, frame.up);

    const auto startRecord = legRecordAt(leg, start);
    const auto endRecord = legRecordAt(leg, end);
    REQUIRE(startRecord);
    REQUIRE(endRecord);
    const cwStationLrud low = recordAcross(startRecord->first, startRecord->second, frame);
    const cwStationLrud high = recordAcross(endRecord->first, endRecord->second, frame);
    const double t = std::clamp(static_cast<double>(alongPoint), 0.0, 1.0);

    const double x = QVector3D::dotProduct(offset, frame.right);
    const double y = QVector3D::dotProduct(offset, frame.up);
    const std::optional<double> a = x >= 0.0 ? lerpReading(low.right, high.right, t) : lerpReading(low.left, high.left, t);
    const std::optional<double> b = y >= 0.0 ? lerpReading(low.up, high.up, t) : lerpReading(low.down, high.down, t);
    LegWall result {offset.length(), std::nullopt};
    if (a && b) {
        const double theta = std::atan2(y, x);
        result.wall = *a * *b / std::hypot(*b * std::cos(theta), *a * std::sin(theta));
    }
    return result;
}

// The point's offset from, and wall of, whichever of the corner's two legs
// has the nearer extended axis.
LegWall nearerLegWall(const Builder::Debug& debug, const Builder::Chain& chain, const Corner& corner, const QVector3D& point)
{
    const LegWall incoming = legWallAt(debug, chain, corner.incomingLeg, point);
    const LegWall outgoing = legWallAt(debug, chain, corner.outgoingLeg, point);
    return incoming.offset <= outgoing.offset ? incoming : outgoing;
}

// Every measured bin of an elbow ring lies on the corner's envelope: the
// union of the two leg tubes, each run on past the corner station at the
// station's reading as far as it lies inside the other. A vertex is on the
// envelope when it is on one leg's wall where that leg's tube reaches, and
// lies short of the interior of either tube's reach.
void checkElbowRingOnLegWalls(const Builder::Debug& debug, const Builder::Chain& chain, int ringIndex, const Corner& corner)
{
    constexpr double kTolerance = cwPassageTubeMeshCheck::kWallTolerance;
    const Builder::Ring& ring = chain.rings.at(ringIndex);
    for (int bin = 0; bin < ring.radii.size(); ++bin) {
        if (!ring.measured.at(bin)) {
            continue;
        }
        const QVector3D vertex = ring.center + binDirection(ring, bin) * static_cast<float>(ring.radii.at(bin));
        const LegWall incoming = legWallAt(debug, chain, corner.incomingLeg, vertex);
        const LegWall outgoing = legWallAt(debug, chain, corner.outgoingLeg, vertex);
        if (!incoming.wall || !outgoing.wall) {
            continue;
        }
        const double pastStation = QVector3D::dotProduct(vertex - corner.station, corner.incoming);
        const double beforeStation = -QVector3D::dotProduct(vertex - corner.station, corner.outgoing);
        const auto isOnWall = [](const LegWall& leg) {
            return std::abs(leg.offset - *leg.wall) <= kTolerance;
        };
        const auto isWithin = [](const LegWall& leg, double margin) {
            return leg.offset <= *leg.wall + margin;
        };
        const bool isOnIncoming = isOnWall(incoming) && (pastStation <= kTolerance || isWithin(outgoing, kTolerance));
        const bool isOnOutgoing = isOnWall(outgoing) && (beforeStation <= kTolerance || isWithin(incoming, kTolerance));
        const bool isInsideIncoming = isWithin(incoming, -kTolerance) && (pastStation < -kTolerance || isWithin(outgoing, -kTolerance));
        const bool isInsideOutgoing = isWithin(outgoing, -kTolerance) && (beforeStation < -kTolerance || isWithin(incoming, -kTolerance));
        INFO("corner " << corner.name.toStdString() << " elbow ring " << ringIndex << " bin " << bin
                       << ": vertex " << incoming.offset << " m off the incoming leg's axis, its wall " << *incoming.wall
                       << ", " << outgoing.offset << " m off the outgoing leg's axis, its wall " << *outgoing.wall
                       << ", " << pastStation << " m past the station");
        CHECK((isOnIncoming || isOnOutgoing));
        CHECK_FALSE((isInsideIncoming || isInsideOutgoing));
    }
}

// Checks each ring of the chain on a leg between two of stations: rings
// along a leg against their two stations' readings, elbow rings of a corner
// between two such legs against the two legs' walls. Returns how many rings
// it checked.
int checkRingsOnLegWalls(const Builder::Debug& debug, const Builder::Chain& chain, const QStringList& stations)
{
    const auto isLegBetween = [&](int chainLeg) {
        const Builder::Leg& leg = debug.legs.at(chain.legs.at(chainLeg).legIndex);
        return stations.contains(leg.from) && stations.contains(leg.to);
    };
    int checkedRings = 0;
    for (int i = 0; i < chain.rings.size(); ++i) {
        const Builder::Ring& ring = chain.rings.at(i);
        if (ring.isElbow) {
            const Corner corner = nearestCorner(debug, chain, ring.center);
            if (isLegBetween(corner.incomingLeg) && isLegBetween(corner.outgoingLeg) && ring.frame.inclinationWeight == 0.0) {
                checkElbowRingOnLegWalls(debug, chain, i, corner);
                ++checkedRings;
            }
        } else if (isOnLegBetween(debug, chain, ring, stations)) {
            if (const std::optional<RingWalls> walls = ringWalls(debug, chain, i)) {
                checkWallsWithinReadings(*walls);
                ++checkedRings;
            }
        }
    }
    return checkedRings;
}

// Where the ring's horizontal axis meets its wall on the inside of the
// corner's bend, or on the outside.
QVector3D sideWallPoint(const Builder::Ring& ring, const Corner& corner, bool isInside)
{
    const bool rightIsInside = QVector3D::dotProduct(ring.frame.right, corner.inside()) > 0.0f;
    const float sign = rightIsInside == isInside ? 1.0f : -1.0f;
    const std::optional<WallHit> hit = wallHitAlong(ring, QVector2D(sign, 0.0f));
    REQUIRE(hit);
    return ring.center + ring.frame.right * (sign * static_cast<float>(hit->reach));
}

const Builder::Ring& elbowMiddleRing(const Builder::Chain& chain, const QVector3D& station)
{
    const auto it = std::find_if(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& ring) {
        return ring.isElbow && ring.section && ring.section->station.distanceToPoint(station) < kSameStationMeters;
    });
    REQUIRE(it != chain.rings.cend());
    return *it;
}

QVector3D legPosition(const Builder::Debug& debug, const QString& station)
{
    for (const Builder::Leg& leg : debug.legs) {
        if (leg.from == station) {
            return leg.fromPosition;
        }
        if (leg.to == station) {
            return leg.toPosition;
        }
    }
    FAIL("no leg at " << station.toStdString());
    return {};
}

QString wallsText(const QVector<double>& walls)
{
    QStringList text;
    for (double wall : walls) {
        text.append(QString::number(wall, 'f', 2));
    }
    return text.join(QStringLiteral(" -> "));
}

} // namespace

// A corner's walls are the two leg tubes, each at its interpolated width and
// run on past the corner station at the station's reading as far as it stays
// inside the other: a miter outside, a square corner inside. Every elbow
// ring reaches from its own center to that envelope, through the S-bend of
// case 30, Phake Cave's a1 -> a4 chain and the top of case 25's shaft.
TEST_CASE("cwPassageTubeBuilder keeps a corner's walls on the legs' walls", "[PassageTube][cwPassageTubeBuilder]")
{
    SECTION("S-bend")
    {
        constexpr double kS1Reading = 3.0;
        constexpr double kMonotonicTolerance = 1e-3;
        const cwPassageTubeCases::Case& entry = catalogCase(QStringLiteral("30. S-bend from a small to a large to a medium section"));
        const Builder::Debug debug = buildCase(entry);
        REQUIRE(debug.chains.size() == 1);
        const Builder::Chain& chain = debug.chains.at(0);
        CHECK(checkRingsOnLegWalls(debug, chain, {QStringLiteral("s0"), QStringLiteral("s1"), QStringLiteral("s2"), QStringLiteral("s3")}) > chain.legs.size());

        const QVector3D s0 = casePosition(entry, QStringLiteral("s0"));
        const QVector3D s1 = casePosition(entry, QStringLiteral("s1"));
        const Corner corner = nearestCorner(debug, chain, s1);
        REQUIRE(corner.name == QStringLiteral("s1"));

        // The outside wall before s1 runs on the incoming leg's wall, rising
        // toward s1's reading without a bulge, and meets the outgoing leg's
        // wall in a miter on the bisector.
        QVector<double> outsideBefore;
        for (const Builder::Ring& ring : chain.rings) {
            const QVector3D outside = sideWallPoint(ring, corner, false);
            const float along = QVector3D::dotProduct(outside - s0, corner.incoming);
            if (along > 0.0f && along < s0.distanceToPoint(s1)) {
                const LegWall wall = legWallAt(debug, chain, corner.incomingLeg, outside);
                REQUIRE(wall.wall);
                INFO("outside wall " << wall.offset << " at " << along << " m along, the leg's wall " << *wall.wall);
                CHECK(std::abs(wall.offset - *wall.wall) <= cwPassageTubeMeshCheck::kWallTolerance);
                outsideBefore.append(wall.offset);
            }
        }
        REQUIRE_FALSE(outsideBefore.isEmpty());
        INFO("s1 outside walls before the station " << wallsText(outsideBefore).toStdString());
        for (int i = 1; i < outsideBefore.size(); ++i) {
            CHECK(outsideBefore.at(i) >= outsideBefore.at(i - 1) - kMonotonicTolerance);
        }
        const Builder::Ring& middle = elbowMiddleRing(chain, s1);
        const double halfTurn = std::acos(static_cast<double>(QVector3D::dotProduct(corner.incoming, corner.outgoing))) / 2.0;
        const QVector3D miter = sideWallPoint(middle, corner, false);
        CHECK(miter.distanceToPoint(s1) == Catch::Approx(kS1Reading / std::cos(halfTurn)).margin(cwPassageTubeMeshCheck::kWallTolerance));

        // The inside wall from the inset ring before s1 to the one after,
        // off the nearer leg's axis: rising to the middle ring, falling after.
        QVector<int> elbow;
        for (int i = 0; i < chain.rings.size(); ++i) {
            if (chain.rings.at(i).isElbow && nearestCorner(debug, chain, chain.rings.at(i).center).name == corner.name) {
                elbow.append(i);
            }
        }
        REQUIRE_FALSE(elbow.isEmpty());
        REQUIRE(elbow.first() > 0);
        REQUIRE(elbow.last() + 1 < chain.rings.size());
        QVector<double> inside;
        int middleIndex = -1;
        for (int i = elbow.first() - 1; i <= elbow.last() + 1; ++i) {
            const Builder::Ring& ring = chain.rings.at(i);
            if (&ring == &middle) {
                middleIndex = inside.size();
            }
            inside.append(nearerLegWall(debug, chain, corner, sideWallPoint(ring, corner, true)).offset);
        }
        REQUIRE(middleIndex >= 0);
        INFO("s1 inside walls " << wallsText(inside).toStdString());
        for (int i = 1; i < inside.size(); ++i) {
            INFO("inside wall " << i);
            if (i <= middleIndex) {
                CHECK(inside.at(i) >= inside.at(i - 1) - kMonotonicTolerance);
            } else {
                CHECK(inside.at(i) <= inside.at(i - 1) + kMonotonicTolerance);
            }
        }
    }

    SECTION("Phake Cave a1 to a4")
    {
        constexpr double kA2Inside = 2.5;
        const Builder::Debug debug = buildDebug(makeInput(cwPassageTubeCases::phakeCave()));
        const Builder::Chain& chain = chainWithLeg(debug, QStringLiteral("a1"), QStringLiteral("a2"));
        const QStringList stations {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3"), QStringLiteral("a4")};
        CHECK(checkRingsOnLegWalls(debug, chain, stations) > chain.legs.size());

        const QVector3D a2 = legPosition(debug, QStringLiteral("a2"));
        const Corner corner = nearestCorner(debug, chain, a2);
        REQUIRE(corner.name == QStringLiteral("a2"));
        const Builder::Ring& middle = elbowMiddleRing(chain, a2);
        const double inside = nearerLegWall(debug, chain, corner, sideWallPoint(middle, corner, true)).offset;
        INFO("a2 middle ring's inside wall " << inside);
        CHECK(inside == Catch::Approx(kA2Inside).margin(cwPassageTubeMeshCheck::kWallTolerance));
    }

    SECTION("Top of a shaft")
    {
        constexpr double kLeadInCeiling = 2.0;
        constexpr double kShaftHalfWidth = 2.0;
        constexpr double kLeadInFloor = 1.0;
        const cwPassageTubeCases::Case& entry = catalogCase(QStringLiteral("25. Zero floor reading at the foot of a shaft"));
        const Builder::Debug debug = buildCase(entry);
        const Builder::Chain& chain = chainWithLeg(debug, QStringLiteral("f0"), QStringLiteral("f1"));
        const QVector3D f0 = casePosition(entry, QStringLiteral("f0"));
        const QVector3D f1 = casePosition(entry, QStringLiteral("f1"));

        // The lead-in's floor meets the shaft's south wall in a square corner,
        // which the elbow rings reach and no vertex reaches past.
        const QVector2D insideCorner(f1.y() - kShaftHalfWidth, f0.z() - kLeadInFloor);
        double nearestToCorner = std::numeric_limits<double>::max();
        int checkedRings = 0;
        for (int i = 0; i < chain.rings.size(); ++i) {
            const Builder::Ring& ring = chain.rings.at(i);
            if (!ring.isElbow || nearestCorner(debug, chain, ring.center).name != QStringLiteral("f1")) {
                continue;
            }
            ++checkedRings;
            const auto highest = std::max_element(ring.vertices.cbegin(), ring.vertices.cend(), [](const QVector3D& a, const QVector3D& b) {
                return a.z() < b.z();
            });
            const auto northernmost = std::max_element(ring.vertices.cbegin(), ring.vertices.cend(), [](const QVector3D& a, const QVector3D& b) {
                return a.y() < b.y();
            });
            REQUIRE(highest != ring.vertices.cend());
            INFO("f1 elbow ring " << i << ": highest vertex " << highest->z() - f0.z() << " m over the lead-in's axis, northernmost "
                                  << northernmost->y() - f0.y() << " m north");
            CHECK(highest->z() - f0.z() <= kLeadInCeiling + cwPassageTubeMeshCheck::kWallTolerance);
            CHECK(northernmost->y() - f0.y() <= f1.y() - f0.y() + kShaftHalfWidth + cwPassageTubeMeshCheck::kWallTolerance);

            for (const QVector3D& vertex : ring.vertices) {
                INFO("vertex at " << vertex.y() << " north, " << vertex.z() << " up");
                CHECK((vertex.y() >= insideCorner.x() - cwPassageTubeMeshCheck::kWallTolerance
                       || vertex.z() >= insideCorner.y() - cwPassageTubeMeshCheck::kWallTolerance));
                nearestToCorner = std::min(nearestToCorner, static_cast<double>((QVector2D(vertex.y(), vertex.z()) - insideCorner).length()));
            }
        }
        CHECK(checkedRings > 0);
        INFO("the nearest f1 elbow vertex lies " << nearestToCorner << " m from the lead-in floor's corner with the shaft");
        CHECK(nearestToCorner <= cwPassageTubeMeshCheck::kWallTolerance);
    }
}

// b–c and c–d belong to the second trip, read only at d; b's ring, read in
// the first trip, lies nearer the rings on b–c but they borrow d's.
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
    REQUIRE(debug.chains.size() == 1);

    const Builder::Ring& ring = ringAt(debug.chains.at(0), QVector3D(12, 0, 0));
    for (double radius : ring.radii) {
        CHECK(radius == Catch::Approx(kWide));
    }
}

TEST_CASE("cwPassageTubeBuilder breaks a chain at a hairpin", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kHairpinDegrees = 170.0;
    constexpr float kLegMeters = 10.0f;
    const QVector3D back(static_cast<float>(std::cos(qDegreesToRadians(kHairpinDegrees))),
                         static_cast<float>(std::sin(qDegreesToRadians(kHairpinDegrees))),
                         0.0f);
    const QStringList dimensions = lrud(0.5, 0.5, 0.5, 0.5);
    const QVector3D hairpin(kLegMeters, 0, 0);
    const QVector3D turned = hairpin + back * kLegMeters;
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), QVector3D(0, 0, 0), dimensions},
        {QStringLiteral("b"), hairpin, dimensions},
        {QStringLiteral("c"), turned, dimensions},
        {QStringLiteral("d"), turned + QVector3D(-kLegMeters, 0, 0), dimensions},
    }}});
    const Builder::Debug debug = buildDebug(input);

    REQUIRE(debug.chains.size() == 2);
    checkChainsCoverLegs(debug);
    for (const Builder::Chain& chain : debug.chains) {
        const QStringList stations = chainStations(debug, chain);
        INFO(stations.join(QStringLiteral(" ")).toStdString());
        const bool startsAtHairpin = stations.first() == QStringLiteral("b");
        CHECK((startsAtHairpin || stations.last() == QStringLiteral("b")));
        CHECK(chain.startCap == (startsAtHairpin ? Builder::EndCap::Dome : Builder::EndCap::Fan));
        CHECK(chain.endCap == (startsAtHairpin ? Builder::EndCap::Fan : Builder::EndCap::Dome));
    }

    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(std::none_of(report.issues.cbegin(), report.issues.cend(), [](const cwPassageTubeMeshCheck::Issue& issue) {
        return issue.type == cwPassageTubeMeshCheck::Crease;
    }));
    checkMeshClosed(debug.mesh);
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

// The arm walks d → b, south into the through tube, and ends at b in a
// mouth zipped into the through tube.
const Builder::Chain& junctionArm(const Builder::Debug& debug)
{
    REQUIRE(debug.chains.size() == 2);
    const Builder::Chain& arm = debug.chains.at(1);
    REQUIRE(chainStations(debug, arm) == QStringList({QStringLiteral("d"), QStringLiteral("b")}));
    CHECK(arm.startCap == Builder::EndCap::Fan);
    CHECK(arm.endCap == Builder::EndCap::Mouth);
    CHECK(arm.endMouth.size() == Builder::Options().ringResolution);
    CHECK(debug.chains.at(0).windows.size() == 1);
    return arm;
}

} // namespace

TEST_CASE("cwPassageTubeBuilder closes a junction arm with a mouth on the through passage", "[PassageTube][cwPassageTubeBuilder]")
{
    const Builder::Debug debug = buildDebug(junctionInput(lrud(2, 2, 2, 2)));
    const Builder::Chain& arm = junctionArm(debug);

    // b's 2 m walls make the through a 2 m cylinder about the x axis, which
    // the arm's default-radius section meets north of b.
    for (const QVector3D& point : mouthPositions(arm.endMouth)) {
        CHECK(point.y() > 0.0f);
        CHECK(std::hypot(point.y(), point.z()) == Catch::Approx(2.0).margin(0.05));
    }
    for (const Builder::Ring& ring : arm.rings) {
        CHECK(ring.center.y() > 2.0f);
    }
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

// a–b–c along +x, a→b in the first trip and b→c in the second. The second
// trip reads b as wide as the first, or twice as wide.
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
    CHECK(cwPassageTubeMeshCheck::allTripsVisible(mesh).count == next);

    REQUIRE(mesh.tripBoundaryRanges.size() == 2);
    for (const cwPassageTubeMesh::TripBoundary& boundary : mesh.tripBoundaryRanges) {
        CHECK(boundary.range.first == next);
        CHECK(boundary.range.count > 0);
        CHECK(boundary.trip + boundary.neighborTrip == 1);
        next = boundary.range.first + boundary.range.count;
    }
    CHECK(next == mesh.indices.size());

    CHECK(visibleCapStations(debug) == QStringList({QStringLiteral("a"), QStringLiteral("c")}));
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

// The passage a–b–c of the two-trip test above, with b's 2 m walls, and a
// third trip's arm from d, north of b, joining at b: the window around b
// removes side quads on both sides of b's ring.
TEST_CASE("cwPassageTubeBuilder hides a trip boundary disc inside a junction window", "[PassageTube][cwPassageTubeBuilder]")
{
    const QStringList junctionDimensions = lrud(2, 2, 2, 2);
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0)};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), junctionDimensions};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0)};
    const StationInput d {QStringLiteral("d"), QVector3D(10, 10, 0)};
    const StationInput armB {QStringLiteral("b"), QVector3D(10, 0, 0)};
    const BuilderInput input = cwPassageTubeCases::toSurvey(QList<SurveyInput> {{{{a, b}}}, {{{b, c}}}, {{{d, armB}}}});

    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 2);
    REQUIRE(debug.chains.at(0).windows.size() == 1);
    CHECK(debug.chains.at(1).endCap == Builder::EndCap::Mouth);
    REQUIRE(debug.mesh.tripBoundaryRanges.size() == 2);

    constexpr float kPositionTolerance = 1e-4f;
    // Each trip's disc at b leaves out the ring vertices whose side quads on
    // that trip's side the window removed on both neighboring columns.
    const Builder::Chain& through = debug.chains.at(0);
    const auto atB = std::min_element(through.rings.cbegin(), through.rings.cend(), [&](const Builder::Ring& x, const Builder::Ring& y) {
        return x.center.distanceToPoint(*b.position) < y.center.distanceToPoint(*b.position);
    });
    const int ringAtB = static_cast<int>(atB - through.rings.cbegin());
    REQUIRE(ringAtB > 0);
    const QSet<QPair<int, int>>& removedQuads = through.windows.first().removedQuads;
    const QUuid firstTripId = input.region.caves.at(0).trips.at(0).id;
    const int resolution = static_cast<int>(atB->vertices.size());
    int clippedVertices = 0;
    for (const cwPassageTubeMesh::TripBoundary& boundary : debug.mesh.tripBoundaryRanges) {
        const bool isFirstTrip = debug.mesh.tripUuids.at(boundary.trip) == firstTripId;
        const int quadRing = isFirstTrip ? ringAtB - 1 : ringAtB;
        INFO("disc of trip " << boundary.trip << " against quads of ring " << quadRing);
        QVector<QVector3D> discPositions;
        for (int i = boundary.range.first; i < boundary.range.first + boundary.range.count; ++i) {
            discPositions.append(debug.mesh.positions.at(debug.mesh.indices.at(i)));
        }
        for (int k = 0; k < resolution; ++k) {
            const int previousColumn = (k + resolution - 1) % resolution;
            if (removedQuads.contains({quadRing, previousColumn}) && removedQuads.contains({quadRing, k})) {
                ++clippedVertices;
                const QVector3D& vertex = atB->vertices.at(k);
                CHECK(std::none_of(discPositions.cbegin(), discPositions.cend(), [&](const QVector3D& position) {
                    return position.distanceToPoint(vertex) < kPositionTolerance;
                }));
            }
        }
    }
    CHECK(clippedVertices > 0);

    CHECK_FALSE(visibleCapStations(debug).contains(QStringLiteral("b")));
    checkNoCapExposed(debug);
    checkMeshClean(debug);
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
        CHECK((endsAtTieIn ? chain.endCap : chain.startCap) == Builder::EndCap::Dome);

        // The ring that closes the dome lies past c along the leg reaching c.
        const Builder::Ring& endRing = endsAtTieIn ? chain.rings.last() : chain.rings.first();
        const QVector3D towardTieIn = endsAtTieIn ? endRing.frame.tangent : -endRing.frame.tangent;
        CHECK(QVector3D::dotProduct(endRing.center - tieIn, towardTieIn) > 0.5f);
    }

    CHECK(visibleCapStations(debug) == QStringList({QStringLiteral("a"), QStringLiteral("d")}));
    checkNoCapExposed(debug);
    checkCenterlineInside(debug);
    checkMeshClean(debug);
}

// A Y at J whose three 12 m arms end at K0, K1 and K2. A straight passage
// crosses each K between two dead ends, so the walks from those dead ends
// pass every K before J's own seed. The arms come first in the survey, so J
// seeds before the Ks.
TEST_CASE("cwPassageTubeBuilder chains through a junction no walk reaches first", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr float kArmMeters = 12.0f;
    constexpr float kCrossingMeters = 6.0f;
    constexpr double kArmDegrees[] = {90.0, 210.0, 330.0};
    constexpr double kRightAngleDegrees = 90.0;
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const StationInput junction {QStringLiteral("J"), QVector3D(0, 0, 0), dimensions};

    QList<QList<StationInput>> arms;
    QList<QList<StationInput>> crossings;
    for (int i = 0; i < 3; ++i) {
        const QVector3D far = planDirection(kArmDegrees[i]) * kArmMeters;
        const QVector3D across = planDirection(kArmDegrees[i] + kRightAngleDegrees) * kCrossingMeters;
        const StationInput k {QStringLiteral("K%1").arg(i), far, dimensions};
        arms.append({junction, k});
        crossings.append({{QStringLiteral("L%1").arg(i), far - across, dimensions}, k, {QStringLiteral("R%1").arg(i), far + across, dimensions}});
    }
    const Builder::Debug debug = buildDebug(makeInput({arms + crossings}));

    checkChainsCoverLegs(debug);
    int passing = 0;
    int endingAtJunction = 0;
    for (const Builder::Chain& chain : debug.chains) {
        const QStringList stations = chainStations(debug, chain);
        INFO(stations.join(QStringLiteral(" ")).toStdString());
        if (stations.mid(1, stations.size() - 2).contains(QStringLiteral("J"))) {
            ++passing;
        }
        if (stations.first() == QStringLiteral("J")) {
            ++endingAtJunction;
            CHECK(chain.startCap == Builder::EndCap::Mouth);
            CHECK_FALSE(chain.startMouth.isEmpty());
        }
        if (stations.last() == QStringLiteral("J")) {
            ++endingAtJunction;
            CHECK(chain.endCap == Builder::EndCap::Mouth);
            CHECK_FALSE(chain.endMouth.isEmpty());
        }
    }
    CHECK(passing == 1);
    CHECK(endingAtJunction == 1);
    CHECK_FALSE(visibleCapStations(debug).contains(QStringLiteral("J")));
    checkNoCapExposed(debug);
    checkCenterlineInside(debug);
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
    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).legs.size() == 3);
    checkChainsCoverLegs(debug);
    CHECK(debug.chains.at(0).windows.isEmpty());
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::JunctionUnzipped) == 0);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::JunctionOverlap) == 0);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::RepeatedShot) == 1);

    const auto resurveyedLeg = std::find_if(debug.legs.cbegin(), debug.legs.cend(), [](const Builder::Leg& leg) {
        return leg.from == QStringLiteral("b") && leg.to == QStringLiteral("c");
    });
    REQUIRE(resurveyedLeg != debug.legs.cend());
    CHECK(resurveyedLeg->tripId == input.region.caves.at(0).trips.at(0).id);
    CHECK(*resurveyedLeg->fromLrud.left == Catch::Approx((1.0 + kResurveyScale) / 2.0));

    CHECK(visibleCapStations(debug) == QStringList({QStringLiteral("a"), QStringLiteral("d")}));
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

TEST_CASE("cwPassageTubeBuilder rounds every corner of a square loop with elbow rings", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr float kCornerReachMeters = 2.0f;
    constexpr int kRingsPerRightAngle = 3;
    const QStringList dimensions = lrud(1, 1, 1, 1);
    const QVector<QVector3D> corners {QVector3D(0, 0, 0), QVector3D(10, 0, 0), QVector3D(10, 10, 0), QVector3D(0, 10, 0)};
    const BuilderInput input = makeInput({{{
        {QStringLiteral("a"), corners.at(0), dimensions},
        {QStringLiteral("b"), corners.at(1), dimensions},
        {QStringLiteral("c"), corners.at(2), dimensions},
        {QStringLiteral("d"), corners.at(3), dimensions},
        {QStringLiteral("a"), corners.at(0), dimensions},
    }}});
    const Builder::Debug debug = buildDebug(input);
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);
    REQUIRE(chain.startCap == Builder::EndCap::None);

    for (const QVector3D& corner : corners) {
        INFO("corner " << corner.x() << ", " << corner.y());
        const auto elbowRingCount = std::count_if(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& ring) {
            return ring.isElbow && ring.center.distanceToPoint(corner) < kCornerReachMeters;
        });
        CHECK(elbowRingCount >= kRingsPerRightAngle);
    }
    for (int i = 1; i < chain.rings.size(); ++i) {
        INFO("ring " << i);
        CHECK(isInFrontOf(chain.rings.at(i), chain.rings.at(i - 1)));
    }
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder keeps the inner wall of a sharp bend clean", "[PassageTube][cwPassageTubeBuilder]")
{
    const QList<std::pair<QString, BuilderInput>> bends = {
        {QStringLiteral("vertical pit"), verticalPitInput()},
        {QStringLiteral("pit on a new bearing"), pitOnNewBearingInput()},
        {QStringLiteral("90 degree bend, radius 3"), rightAngleBendInput(3.0)},
        {QStringLiteral("95 degree bend"), sharpestBendInput()},
        {QStringLiteral("loop without caps"), loopInput()},
        {QStringLiteral("loop closing at a sharp bend"), loopInput(lrud(3, 3, 3, 3))},
    };
    for (const auto& [name, input] : bends) {
        INFO(name.toStdString());
        const Builder::Debug debug = buildDebug(input);
        checkMeshClean(debug);
        checkNoVertexPulled(debug);
    }
}

TEST_CASE("cwPassageTubeBuilder widens the corner inset with the ring radius", "[PassageTube][cwPassageTubeBuilder]")
{
    static constexpr double kWideRadius = 3.0;
    static constexpr double kNarrowRadius = 0.3;
    static constexpr double kWideInset = 3.3;
    static constexpr double kNarrowInset = 0.5;
    const auto [radius, expectedInset] = GENERATE(table<double, double>({
        {kWideRadius, kWideInset},
        {kNarrowRadius, kNarrowInset},
    }));
    INFO("radius " << radius);
    const Builder::Debug debug = buildDebug(rightAngleBendInput(radius, 20.0f));
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    // The last ring square to each leg before the elbow.
    const QVector<int> elbow = elbowRings(chain, QVector3D(1, 0, 0), QVector3D(0, 1, 0));
    REQUIRE_FALSE(elbow.isEmpty());
    REQUIRE(elbow.first() > 0);
    REQUIRE(elbow.last() + 1 < chain.rings.size());
    CHECK(chain.rings.at(elbow.first() - 1).center.distanceToPoint(kBendCorner) == Catch::Approx(expectedInset).margin(0.05));
    CHECK(chain.rings.at(elbow.last() + 1).center.distanceToPoint(kBendCorner) == Catch::Approx(expectedInset).margin(0.05));
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder clamps the inner wall of a bend on a short leg", "[PassageTube][cwPassageTubeBuilder]")
{
    // 4 m legs cap the inset at 0.45 × 4 = 1.8 m, short of the 3.3 m the
    // 3 m walls need, so the inner wall is pulled in to 1.8 × 0.95 m.
    constexpr double kRadius = 3.0;
    constexpr float kLegLength = 4.0f;
    constexpr double kInset = 1.8;
    constexpr double kMaxInnerRadius = kInset * 0.95 + 1e-3;
    const Builder::Debug debug = buildDebug(rightAngleBendInput(kRadius, kLegLength));
    REQUIRE(debug.chains.size() == 1);
    const Builder::Chain& chain = debug.chains.at(0);

    const QVector3D incoming(1, 0, 0);
    const QVector3D outgoing(0, 1, 0);
    const QVector3D inside = (outgoing - incoming).normalized();
    const auto checkInnerSide = [&](const Builder::Ring& ring, const QVector3D& legDirection) {
        INFO("ring at (" << ring.center.x() << ", " << ring.center.y() << ")");
        for (const QVector3D& vertex : ring.vertices) {
            const QVector3D offset = vertex - kBendCorner;
            if (QVector3D::dotProduct(vertex - ring.center, inside) > 1e-4f) {
                // Distance from the leg's axis: the radius of a ring square to
                // the leg, the section of the corner ring across the leg.
                const QVector3D across = offset - legDirection * QVector3D::dotProduct(offset, legDirection);
                CHECK(across.length() <= kMaxInnerRadius);
            }
        }
    };

    const Builder::Ring& before = ringAt(chain, kBendCorner - incoming * static_cast<float>(kInset));
    const Builder::Ring& after = ringAt(chain, kBendCorner + outgoing * static_cast<float>(kInset));
    checkInnerSide(before, incoming);
    checkInnerSide(after, outgoing);
    QVector<const Builder::Ring*> capped {&before, &after};
    // The elbow's arc has radius inset / tan 45°, so its rings are capped
    // the same.
    const QVector<int> elbow = elbowRings(chain, incoming, outgoing);
    CHECK(elbow.size() == 3);
    for (int index : elbow) {
        capped.append(&chain.rings.at(index));
    }
    for (const Builder::Ring* ring : std::as_const(capped)) {
        INFO("ring at (" << ring->center.x() << ", " << ring->center.y() << ")");
        for (int bin = 0; bin < ring->radii.size(); ++bin) {
            if (QVector3D::dotProduct(binDirection(*ring, bin), inside) > 1e-4f) {
                CHECK(ring->radii.at(bin) <= kMaxInnerRadius);
                CHECK_FALSE(ring->measured.at(bin));
            }
        }
    }

    const auto clampWarnings = std::count_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == cwPassageTubeMesh::Warning::BendClamped;
    });
    CHECK(clampWarnings == 1);
    checkMeshClean(debug);
    checkNoVertexPulled(debug);
}

TEST_CASE("cwPassageTubeBuilder keeps the Phake Cave a11 to a12 leg a single sane tube", "[PassageTube][cwPassageTubeBuilder]")
{
    // The largest wall readings on the leg: a11's left, a12's right and
    // ceiling. a12's 35 m down is the depth of an unsurveyed pit, not a wall.
    constexpr double kMaxLeftMeters = 1.0;
    constexpr double kMaxRightMeters = 2.0;
    constexpr double kMaxRadiusMeters = 8.0;
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
    constexpr double kJunctionTolerance = 0.02;
    constexpr double kOnMeshMeters = 1e-4;

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
    const int armIndex = chainWithLeg(QStringLiteral("a13"), QStringLiteral("a10"));
    REQUIRE(armIndex != through);
    const Builder::Chain& arm = debug.chains.at(armIndex);
    const QVector<QVector3D> mouth = mouthPositions(arm.endMouth.isEmpty() ? arm.startMouth : arm.endMouth);
    REQUIRE(mouth.size() >= Builder::Options().ringResolution);

    // The ring tube blends between rings while the mesh lofts straight
    // quads, which differ across a10's grown rings, so the mouth is held to
    // the through's triangles.
    const cwPassageTubeJunction::RingTube throughTube {&debug.chains.at(through).rings, QVector3D()};
    for (const Builder::Ring& ring : arm.rings) {
        for (const QVector3D& vertex : ring.vertices) {
            INFO("arm vertex (" << vertex.x() << ", " << vertex.y() << ", " << vertex.z() << ")");
            CHECK_FALSE(throughTube.inside(vertex, -kJunctionTolerance));
        }
    }
    for (const QVector3D& point : mouth) {
        INFO("mouth point (" << point.x() << ", " << point.y() << ", " << point.z() << ")");
        CHECK(distanceToSides(debug.chains.at(through), point) < kOnMeshMeters);
    }

    checkMeshClosed(debug.mesh);
}

namespace {

// A straight through passage a → b → c along +x, 20 m long, with arms ending
// at b.
BuilderInput throughWithArms(const QStringList& throughDimensions, const QList<QList<StationInput>>& arms)
{
    const StationInput a {QStringLiteral("a"), QVector3D(0, 0, 0), throughDimensions};
    const StationInput b {QStringLiteral("b"), QVector3D(10, 0, 0), throughDimensions};
    const StationInput c {QStringLiteral("c"), QVector3D(20, 0, 0), throughDimensions};
    SurveyInput survey;
    survey.chunks.append({a, b, c});
    survey.chunks.append(arms);
    return makeInput(survey);
}

QList<StationInput> armFrom(const QString& name, const QVector3D& far, const QStringList& dimensions)
{
    return {StationInput {name, far, dimensions}, StationInput {QStringLiteral("b"), QVector3D(10, 0, 0), dimensions}};
}

} // namespace

TEST_CASE("cwPassageTubeBuilder puts every mouth vertex on the through mesh", "[PassageTube][cwPassageTubeBuilder]")
{
    constexpr double kOnMeshMeters = 1e-4;
    const Builder::Debug debug = buildDebug(throughWithArms(lrud(1, 1, 1, 1),
                                                            {armFrom(QStringLiteral("d"), QVector3D(10, -10, 0), lrud(0.5, 0.5, 0.5, 0.5))}));
    REQUIRE(debug.chains.size() == 2);
    const Builder::Chain& arm = debug.chains.at(1);
    REQUIRE(arm.endCap == Builder::EndCap::Mouth);
    REQUIRE(arm.endMouth.size() == Builder::Options().ringResolution);
    CHECK(debug.chains.at(0).windows.size() == 1);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::JunctionUnzipped) == 0);

    for (const QVector3D& point : mouthPositions(arm.endMouth)) {
        INFO("mouth point (" << point.x() << ", " << point.y() << ", " << point.z() << ")");
        CHECK(distanceToSides(debug.chains.at(0), point) < kOnMeshMeters);
    }
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder welds the mouth vertices of a thin arm", "[PassageTube][cwPassageTubeBuilder]")
{
    // A 7 cm arm's columns meet the through under 3 cm apart, so each pair
    // welds into one mouth vertex.
    constexpr double kWeldMeters = 0.03;
    const Builder::Debug debug = buildDebug(throughWithArms(lrud(1, 1, 1, 1),
                                                            {armFrom(QStringLiteral("d"), QVector3D(10, -10, 0), lrud(0.07, 0.07, 0.07, 0.07))}));
    REQUIRE(debug.chains.size() == 2);
    REQUIRE(debug.chains.at(0).windows.size() == 1);
    REQUIRE(debug.chains.at(0).windows.first().mouths.size() == 1);
    const Builder::WindowMouth& window = debug.chains.at(0).windows.first().mouths.first();
    const QVector<QVector3D> mouth = mouthPositions(debug.chains.at(1).endMouth);
    REQUIRE(window.mouthCorners.size() == mouth.size());

    int welded = 0;
    for (int k = 0; k < window.mouthCorners.size(); ++k) {
        const Builder::ZipCorner& corner = window.mouthCorners.at(k);
        if (corner.ring == Builder::ZipCorner::kMouthRing && corner.column != k) {
            ++welded;
            INFO("column " << k << " welded to " << corner.column);
            CHECK(corner.column < k);
            CHECK(mouth.at(k).distanceToPoint(mouth.at(corner.column)) < 1e-6f);
            CHECK(window.mouthCorners.at(corner.column) == corner);
        }
    }
    CHECK(welded > 0);
    for (int k = 1; k < mouth.size(); ++k) {
        if (window.mouthCorners.at(k) != window.mouthCorners.at(k - 1)) {
            CHECK(mouth.at(k).distanceToPoint(mouth.at(k - 1)) > kWeldMeters);
        }
    }
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder re-grows the through after dropping an arm", "[PassageTube][cwPassageTubeBuilder]")
{
    // The arm's 2.6 m ceiling pokes out of the 2 m through, which grows to
    // take it, but its sides lie inside, so the arm is left out and the
    // through keeps its own section.
    constexpr double kRadiusTolerance = 1e-3;
    const QStringList throughDimensions = lrud(2, 2, 2, 2);
    const Builder::Debug debug = buildDebug(throughWithArms(throughDimensions,
                                                            {armFrom(QStringLiteral("d"), QVector3D(10, -1, 0), lrud(0.3, 0.3, 2.6, 0.3))}));
    const Builder::Debug withoutArm = buildDebug(throughWithArms(throughDimensions, {}));
    REQUIRE(debug.chains.size() == 2);
    REQUIRE(withoutArm.chains.size() == 1);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::ArmInsideThrough) == 1);
    CHECK(debug.chains.at(1).rings.isEmpty());

    const Builder::Ring& plain = withoutArm.chains.at(0).rings.first();
    for (const Builder::Ring& ring : debug.chains.at(0).rings) {
        INFO("ring at x " << ring.center.x());
        CHECK_FALSE(ring.isGrown);
        REQUIRE(ring.radii.size() == plain.radii.size());
        for (int bin = 0; bin < ring.radii.size(); ++bin) {
            CHECK(ring.radii.at(bin) == Catch::Approx(plain.radii.at(bin)).margin(kRadiusTolerance));
        }
    }
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeBuilder hides the fallback cone inside the through", "[PassageTube][cwPassageTubeBuilder]")
{
    // Two arms 30 degrees apart open through the same wall; the second is
    // capped with a fan whose center sits inside the through.
    constexpr double kSpreadDegrees = 30.0;
    constexpr double kOnMouthMeters = 1e-3;
    const float spread = static_cast<float>(qDegreesToRadians(kSpreadDegrees));
    const QVector3D second(10.0f + 10.0f * std::sin(spread), -10.0f * std::cos(spread), 0.0f);
    const QStringList armDimensions = lrud(0.8, 0.8, 0.8, 0.8);
    const Builder::Debug debug = buildDebug(throughWithArms(lrud(1, 1, 1, 1),
                                                            {armFrom(QStringLiteral("d"), QVector3D(10, -10, 0), armDimensions),
                                                             armFrom(QStringLiteral("e"), second, armDimensions)}));
    REQUIRE(debug.chains.size() == 3);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::JunctionOverlap) == 1);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::JunctionUnzipped) == 0);

    const auto capped = std::find_if(debug.chains.cbegin() + 1, debug.chains.cend(), [](const Builder::Chain& chain) {
        return chain.endCap == Builder::EndCap::Fan;
    });
    REQUIRE(capped != debug.chains.cend());
    REQUIRE(capped->endFanCenter.has_value());
    REQUIRE(capped->endMouth.size() >= Builder::Options().ringResolution);

    const cwPassageTubeMesh& mesh = debug.mesh;
    const auto center = std::find(mesh.positions.cbegin(), mesh.positions.cend(), *capped->endFanCenter);
    REQUIRE(center != mesh.positions.cend());
    const quint32 centerIndex = static_cast<quint32>(center - mesh.positions.cbegin());

    const cwPassageTubeJunction::RingTube throughTube {&debug.chains.at(0).rings, QVector3D()};
    QSet<int> fanTriangles;
    for (int triangle = 0; triangle * 3 < mesh.indices.size(); ++triangle) {
        const quint32 a = mesh.indices.at(triangle * 3);
        const quint32 b = mesh.indices.at(triangle * 3 + 1);
        const quint32 c = mesh.indices.at(triangle * 3 + 2);
        if (a != centerIndex && b != centerIndex && c != centerIndex) {
            continue;
        }
        fanTriangles.insert(triangle);
        const QVector3D centroid = (mesh.positions.at(a) + mesh.positions.at(b) + mesh.positions.at(c)) / 3.0f;
        INFO("fan triangle centroid (" << centroid.x() << ", " << centroid.y() << ", " << centroid.z() << ")");
        const std::optional<double> distance = throughTube.wallDistance(centroid);
        REQUIRE(distance.has_value());
        CHECK(*distance < 0.0);
    }
    CHECK(fanTriangles.size() == capped->endMouth.size());

    // The fan meets the through wall only along its mouth, where its edges
    // lie in the wall's faces.
    const QVector<QVector3D> mouth = mouthPositions(capped->endMouth);
    const auto distanceToMouth = [&](const QVector3D& point) {
        double nearest = std::numeric_limits<double>::max();
        for (int k = 0; k < mouth.size(); ++k) {
            const QVector3D& a = mouth.at(k);
            const QVector3D& b = mouth.at((k + 1) % mouth.size());
            const float t = std::clamp(QVector3D::dotProduct(point - a, b - a) / (b - a).lengthSquared(), 0.0f, 1.0f);
            nearest = std::min(nearest, static_cast<double>(point.distanceToPoint(a + (b - a) * t)));
        }
        return nearest;
    };
    const QVector<cwPassageTubeMeshCheck::Issue> crossings = cwPassageTubeMeshCheck::selfIntersections(mesh, cwPassageTubeMeshCheck::wholeMesh(mesh));
    for (const cwPassageTubeMeshCheck::Issue& issue : crossings) {
        if (fanTriangles.contains(issue.triangle) || fanTriangles.contains(issue.otherTriangle)) {
            INFO(cwPassageTubeMeshCheck::toText({{issue}, 0, {}}).toStdString());
            CHECK(distanceToMouth(issue.position) < kOnMouthMeters);
        }
    }
}
