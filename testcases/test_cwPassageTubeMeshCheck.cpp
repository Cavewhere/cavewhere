/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMeshCheck.h"

//Std includes
#include <algorithm>
#include <array>
#include <utility>

namespace {

using Check = cwPassageTubeMeshCheck;
using Cases = cwPassageTubeCases;

constexpr int kBoxTriangleCount = 12;

// The corner of a box at bit x = 1, y = 2, z = 4 of the index.
QVector3D boxCorner(const QVector3D& lo, const QVector3D& hi, int corner)
{
    return QVector3D(corner & 1 ? hi.x() : lo.x(),
                     corner & 2 ? hi.y() : lo.y(),
                     corner & 4 ? hi.z() : lo.z());
}

// An axis-aligned box of 12 triangles wound counter-clockwise from outside.
void appendBox(cwPassageTubeMesh& mesh, const QVector3D& lo, const QVector3D& hi)
{
    constexpr quint32 kBoxTriangles[] = {
        0, 2, 3, 0, 3, 1, // -z
        4, 5, 7, 4, 7, 6, // +z
        0, 1, 5, 0, 5, 4, // -y
        2, 6, 7, 2, 7, 3, // +y
        0, 4, 6, 0, 6, 2, // -x
        1, 3, 7, 1, 7, 5, // +x
    };
    const quint32 first = static_cast<quint32>(mesh.positions.size());
    for (int corner = 0; corner < 8; ++corner) {
        mesh.positions.append(boxCorner(lo, hi, corner));
    }
    for (quint32 index : kBoxTriangles) {
        mesh.indices.append(first + index);
    }
}

cwPassageTubeMesh boxMesh(const QVector3D& lo, const QVector3D& hi)
{
    cwPassageTubeMesh mesh;
    appendBox(mesh, lo, hi);
    return mesh;
}

cwPassageTubeMesh unitCube()
{
    return boxMesh(QVector3D(0, 0, 0), QVector3D(1, 1, 1));
}

void reverseTriangle(cwPassageTubeMesh& mesh, int triangle)
{
    std::swap(mesh.indices[triangle * 3 + 1], mesh.indices[triangle * 3 + 2]);
}

// The tetrahedron p0 p1 p2 p3 with det(p1 − p0, p2 − p0, p3 − p0) > 0.
void appendTetrahedron(cwPassageTubeMesh& mesh, quint32 p0, quint32 p1, quint32 p2, quint32 p3)
{
    mesh.indices.append({p0, p2, p1, p0, p1, p3, p0, p3, p2, p1, p2, p3});
}

QVector<Check::Type> issueTypes(const Check::Report& report)
{
    QVector<Check::Type> types;
    for (const Check::Issue& issue : report.issues) {
        types.append(issue.type);
    }
    return types;
}

void checkOnlyType(const Check::Report& report, Check::Type type, int count)
{
    INFO(Check::toText(report).toStdString());
    CHECK(issueTypes(report) == QVector<Check::Type>(count, type));
}

bool allOfType(const QVector<Check::Issue>& issues, Check::Type type)
{
    return std::all_of(issues.cbegin(), issues.cend(), [type](const Check::Issue& issue) {
        return issue.type == type;
    });
}

bool anyOfType(const QVector<Check::Issue>& issues, Check::Type type)
{
    return std::any_of(issues.cbegin(), issues.cend(), [type](const Check::Issue& issue) {
        return issue.type == type;
    });
}

Cases::Station station(const QString& name, const QVector3D& position, const QString& radius)
{
    return Cases::Station {name, position, {radius, radius, radius, radius}};
}

// Two straight passages on separate chunks that cross at 90° without sharing
// a station: a1-a2 east with 1 m walls and b1-b2 north with 0.8 m walls.
cwPassageTubeBuilder::Debug crossingPassages()
{
    Cases::Trip trip;
    trip.chunks = {
        {station(QStringLiteral("a1"), QVector3D(-5, 0, 0), QStringLiteral("1")),
         station(QStringLiteral("a2"), QVector3D(5, 0, 0), QStringLiteral("1"))},
        {station(QStringLiteral("b1"), QVector3D(0, -5, 0), QStringLiteral("0.8")),
         station(QStringLiteral("b2"), QVector3D(0, 5, 0), QStringLiteral("0.8"))},
    };
    const Cases::Survey survey = Cases::toSurvey(trip);
    return cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, cwPassageTubeBuilder::Options());
}

int nearestVertex(const cwPassageTubeMesh& mesh, const QVector3D& point)
{
    const auto nearest = std::min_element(mesh.positions.cbegin(), mesh.positions.cend(), [&](const QVector3D& a, const QVector3D& b) {
        return a.distanceToPoint(point) < b.distanceToPoint(point);
    });
    return static_cast<int>(std::distance(mesh.positions.cbegin(), nearest));
}

} // namespace

TEST_CASE("cwPassageTubeMeshCheck finds nothing on a closed cube", "[PassageTube][cwPassageTubeMeshCheck]")
{
    const cwPassageTubeMesh mesh = unitCube();
    const Check::Report report = Check::check(mesh);
    INFO(Check::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
    REQUIRE(report.componentCount == 1);
    REQUIRE(report.components.size() == 1);
    const Check::ComponentStats& cube = report.components.first();
    CHECK(cube.vertexCount == 8);
    CHECK(cube.edgeCount == 18);
    CHECK(cube.triangleCount == 12);
    CHECK(cube.genus == 0);
    CHECK(cube.volume == Catch::Approx(1.0));
    CHECK(Check::signedVolume(mesh, Check::wholeMesh(mesh)) == Catch::Approx(1.0));
}

TEST_CASE("cwPassageTubeMeshCheck reports each edge of an open cube's hole", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh mesh = unitCube();
    mesh.indices.remove(mesh.indices.size() - 6, 6);
    const Check::Report report = Check::check(mesh);
    checkOnlyType(report, Check::OpenEdge, 4);
    for (const Check::Issue& issue : report.issues) {
        CHECK(issue.position.x() == 1.0f);
    }
}

TEST_CASE("cwPassageTubeMeshCheck reports the edges around a flipped face", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh mesh = unitCube();
    reverseTriangle(mesh, 10);
    reverseTriangle(mesh, 11);
    checkOnlyType(Check::check(mesh), Check::InconsistentWinding, 4);
}

TEST_CASE("cwPassageTubeMeshCheck reports a vertex joining two tetrahedra", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh mesh;
    mesh.positions = {
        QVector3D(0, 0, 0),
        QVector3D(1, 0, 0), QVector3D(0, 1, 0), QVector3D(0, 0, 1),
        QVector3D(-1, 0, 0), QVector3D(0, -1, 0), QVector3D(0, 0, -1),
    };
    appendTetrahedron(mesh, 0, 1, 2, 3);
    appendTetrahedron(mesh, 0, 4, 6, 5);

    const Check::Report report = Check::check(mesh);
    checkOnlyType(report, Check::NonManifoldVertex, 1);
    CHECK(report.issues.first().position == QVector3D(0, 0, 0));
}

TEST_CASE("cwPassageTubeMeshCheck reports an inside-out cube", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh mesh = unitCube();
    for (int triangle = 0; triangle < mesh.indices.size() / 3; ++triangle) {
        reverseTriangle(mesh, triangle);
    }
    checkOnlyType(Check::check(mesh), Check::NegativeVolume, 1);
}

TEST_CASE("cwPassageTubeMeshCheck reports zero-area and sliver triangles", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh mesh;
    mesh.positions = {
        QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(0, 1, 0),
        QVector3D(2, 0, 0),
        QVector3D(10, 0, 0), QVector3D(5, 0.005f, 0),
    };
    mesh.indices = {0, 1, 2, 0, 1, 3};
    const QVector<Check::Issue> degenerate = Check::degenerateTriangles(mesh, Check::wholeMesh(mesh));
    REQUIRE(degenerate.size() == 1);
    CHECK(degenerate.first().type == Check::DegenerateTriangle);
    CHECK(degenerate.first().triangle == 1);

    mesh.indices = {0, 1, 2, 0, 4, 5};
    const QVector<Check::Issue> sliver = Check::degenerateTriangles(mesh, Check::wholeMesh(mesh));
    REQUIRE(sliver.size() == 1);
    CHECK(sliver.first().type == Check::SliverTriangle);
    CHECK(sliver.first().triangle == 1);
}

TEST_CASE("cwPassageTubeMeshCheck reports interpenetrating boxes and passes them apart", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh crossing = unitCube();
    appendBox(crossing, QVector3D(0.5f, 0.5f, 0.5f), QVector3D(1.5f, 1.5f, 1.5f));
    const Check::Report crossingReport = Check::check(crossing);
    INFO(Check::toText(crossingReport).toStdString());
    CHECK(crossingReport.componentCount == 2);
    CHECK_FALSE(crossingReport.issues.isEmpty());
    CHECK(std::all_of(crossingReport.issues.cbegin(), crossingReport.issues.cend(), [](const Check::Issue& issue) {
        return issue.type == Check::SelfIntersection;
    }));
    for (const Check::Issue& issue : crossingReport.issues) {
        CHECK(issue.triangle < 12);
        CHECK(issue.otherTriangle >= 12);
    }

    cwPassageTubeMesh apart = unitCube();
    appendBox(apart, QVector3D(3, 0, 0), QVector3D(4, 1, 1));
    const Check::Report apartReport = Check::check(apart);
    INFO(Check::toText(apartReport).toStdString());
    CHECK(apartReport.issues.isEmpty());
    CHECK(apartReport.componentCount == 2);
}

TEST_CASE("cwPassageTubeMeshCheck classifies a crossing between two passages as an overlap", "[PassageTube][cwPassageTubeMeshCheck]")
{
    SECTION("Two tubes crossing at 90°") {
        const cwPassageTubeBuilder::Debug debug = crossingPassages();
        REQUIRE(debug.triangleChain.size() == debug.mesh.indices.size() / 3);
        const Check::Report report = Check::check(debug.mesh, debug);
        INFO(Check::toText(report).toStdString());
        CHECK(report.issues.isEmpty());
        CHECK_FALSE(report.warnings.isEmpty());
        CHECK(allOfType(report.warnings, Check::PassageOverlap));
        CHECK(report.warnings.first().detail.contains(QStringLiteral("a1")));
        CHECK(report.warnings.first().detail.contains(QStringLiteral("b1")));
    }

    SECTION("A tube whose ring is folded on itself") {
        cwPassageTubeBuilder::Debug debug = crossingPassages();
        // Pulls a vertex of a1-a2's right wall out through its left wall,
        // away from b1-b2.
        const int folded = nearestVertex(debug.mesh, QVector3D(-3, -1, 0));
        debug.mesh.positions[folded] = QVector3D(-3, 2, 0);
        const QVector<Check::Issue> crossings = Check::selfIntersections(debug.mesh, Check::wholeMesh(debug.mesh), debug);
        INFO(Check::toText({crossings}).toStdString());
        CHECK(anyOfType(crossings, Check::SelfIntersection));
    }

    SECTION("Crossing boxes by the chains that emitted them") {
        cwPassageTubeMesh mesh = unitCube();
        appendBox(mesh, QVector3D(0.5f, 0.5f, 0.5f), QVector3D(1.5f, 1.5f, 1.5f));
        cwPassageTubeBuilder::Debug debug;
        debug.triangleChain = QVector<int>(kBoxTriangleCount, 0) + QVector<int>(kBoxTriangleCount, 1);
        debug.triangleInZipBand = QVector<bool>(2 * kBoxTriangleCount, false);
        debug.legs = {cwPassageTubeBuilder::Leg {}};
        cwPassageTubeBuilder::Chain chain;
        chain.legs = {{0, false}};
        debug.chains = {chain, chain};
        const Check::Range whole = Check::wholeMesh(mesh);

        const QVector<Check::Issue> overlaps = Check::selfIntersections(mesh, whole, debug);
        CHECK_FALSE(overlaps.isEmpty());
        CHECK(allOfType(overlaps, Check::PassageOverlap));

        // A triangle of the first cube's +x face, which the second cube crosses.
        constexpr int kZipTriangle = 10;
        debug.triangleInZipBand[kZipTriangle] = true;
        const QVector<Check::Issue> withZip = Check::selfIntersections(mesh, whole, debug);
        CHECK(anyOfType(withZip, Check::SelfIntersection));
        for (const Check::Issue& issue : withZip) {
            CHECK((issue.type == Check::SelfIntersection) == (issue.triangle == kZipTriangle));
        }

        debug.triangleChain = QVector<int>(2 * kBoxTriangleCount, 0);
        CHECK(allOfType(Check::selfIntersections(mesh, whole, debug), Check::SelfIntersection));
    }
}

TEST_CASE("cwPassageTubeMeshCheck keeps large thin triangles", "[PassageTube][cwPassageTubeMeshCheck]")
{
    // A strip 0.05 m wide along a 3 m leg, where a ring's 0.05 m bin lies
    // beside its 1 m neighbor: aspect ratio 60 at an altitude of 5 cm.
    cwPassageTubeMesh mesh;
    mesh.positions = {QVector3D(0, 0, 0), QVector3D(0, 0.05f, 0), QVector3D(3, 0, 0), QVector3D(3, 0.05f, 0)};
    mesh.indices = {0, 2, 3, 0, 3, 1};
    CHECK(Check::degenerateTriangles(mesh, Check::wholeMesh(mesh)).isEmpty());

    // An altitude of 5 mm under a 1 m edge.
    mesh.positions = {QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(0.5f, 0.005f, 0)};
    mesh.indices = {0, 1, 2};
    const QVector<Check::Issue> sliver = Check::degenerateTriangles(mesh, Check::wholeMesh(mesh));
    REQUIRE(sliver.size() == 1);
    CHECK(sliver.first().type == Check::SliverTriangle);
}

TEST_CASE("cwPassageTubeMeshCheck reports a cube nested in another", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh mesh = boxMesh(QVector3D(0, 0, 0), QVector3D(4, 4, 4));
    appendBox(mesh, QVector3D(1, 1, 1), QVector3D(2, 2, 2));
    const Check::Report report = Check::check(mesh);
    checkOnlyType(report, Check::NestedShell, 1);
    CHECK(report.issues.first().triangle == 12);
    CHECK(report.issues.first().otherTriangle == 0);
}

TEST_CASE("cwPassageTubeMeshCheck reports a sharp fold unless a vertex is an intended crease", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeMesh mesh;
    mesh.positions = {QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(0.5f, 1, 0), QVector3D(0.5f, 0.9f, 0.1f)};
    mesh.indices = {0, 1, 2, 1, 0, 3};
    const Check::Range range = Check::wholeMesh(mesh);

    const QVector<Check::Issue> folded = Check::creases(mesh, range, {});
    REQUIRE(folded.size() == 1);
    CHECK(folded.first().type == Check::Crease);
    CHECK(folded.first().position == QVector3D(0.5f, 0, 0));

    CHECK(Check::creases(mesh, range, {0}).isEmpty());
}

TEST_CASE("cwPassageTubeMeshCheck reports twisted, folded and jumping rings", "[PassageTube][cwPassageTubeMeshCheck]")
{
    constexpr int kResolution = 4;
    const auto ringAt = [](float x, double radius) {
        cwPassageTubeBuilder::Ring ring;
        ring.center = QVector3D(x, 0, 0);
        ring.frame = {QVector3D(1, 0, 0), QVector3D(0, 0, 1), QVector3D(0, -1, 0), 0.0};
        ring.radii = QVector<double>(kResolution, radius);
        ring.measured = QVector<bool>(kResolution, true);
        for (int bin = 0; bin < kResolution; ++bin) {
            const QVector3D directions[] = {ring.frame.right, ring.frame.up, -ring.frame.right, -ring.frame.up};
            ring.vertices.append(ring.center + directions[bin] * static_cast<float>(radius));
        }
        return ring;
    };

    cwPassageTubeBuilder::Debug debug;
    cwPassageTubeBuilder::Leg leg;
    leg.fromPosition = QVector3D(0, 0, 0);
    leg.toPosition = QVector3D(10, 0, 0);
    debug.legs = {leg};
    cwPassageTubeBuilder::Chain chain;
    chain.legs = {{0, false}};
    chain.rings = {ringAt(0, 1), ringAt(1, 1), ringAt(2, 1), ringAt(3, 1), ringAt(10, 1)};
    debug.chains = {chain};
    CHECK(Check::ringIssues(debug).isEmpty());

    SECTION("A ring turned half a turn") {
        cwPassageTubeBuilder::Ring& twisted = debug.chains[0].rings[2];
        twisted.frame.right = -twisted.frame.right;
        const QVector<Check::Issue> issues = Check::ringIssues(debug);
        REQUIRE(issues.size() == 2);
        CHECK(issues.at(0).type == Check::FrameTwist);
        CHECK(issues.at(1).type == Check::FrameTwist);
    }

    SECTION("A vertex behind the previous ring's plane") {
        debug.chains[0].rings[3].vertices[0] = QVector3D(1.5f, -1, 0);
        const QVector<Check::Issue> issues = Check::ringIssues(debug);
        REQUIRE(issues.size() == 1);
        CHECK(issues.first().type == Check::FoldBack);
        CHECK(issues.first().position == QVector3D(1.5f, -1, 0));
    }

    SECTION("A radius jump between samples, but not at a station") {
        debug.chains[0].rings[2].radii[1] = 4.0;
        debug.chains[0].rings[0].radii[1] = 4.0;
        const QVector<Check::Issue> issues = Check::ringIssues(debug);
        REQUIRE(issues.size() == 2);
        CHECK(issues.at(0).type == Check::RadiusJump);
        CHECK(issues.at(0).position == QVector3D(2, 0, 0));
        CHECK(issues.at(1).type == Check::RadiusJump);
        CHECK(issues.at(1).position == QVector3D(3, 0, 0));
    }
}

TEST_CASE("cwPassageTubeMeshCheck reports ring walls beyond the station's readings", "[PassageTube][cwPassageTubeMeshCheck]")
{
    constexpr int kResolution = 4;
    const cwStationLrud lrud {1.0, 2.0, 1.5, 0.5};
    cwPassageTubeBuilder::Leg leg;
    leg.from = QStringLiteral("a");
    leg.to = QStringLiteral("b");
    leg.fromPosition = QVector3D(0, 0, 0);
    leg.toPosition = QVector3D(10, 0, 0);
    leg.fromPassageLrud = lrud;
    leg.toPassageLrud = lrud;
    leg.fromFacing = QVector3D(1, 0, 0);
    leg.toFacing = QVector3D(1, 0, 0);

    // A ring around center whose vertices reach right, up, left and down by
    // the given walls, fitted to the readings of the station at station.
    const auto ringAt = [&](const QVector3D& center, const QVector3D& station, const std::array<float, kResolution>& walls) {
        cwPassageTubeBuilder::Ring ring;
        ring.center = center;
        ring.frame = {QVector3D(1, 0, 0), QVector3D(0, 0, 1), QVector3D(0, -1, 0), 0.0};
        ring.radii = QVector<double>(kResolution, 1.0);
        ring.measured = QVector<bool>(kResolution, true);
        ring.section = cwPassageTubeBuilder::Section {lrud, ring.radii, station};
        const std::array<QVector3D, kResolution> directions {ring.frame.right, ring.frame.up, -ring.frame.right, -ring.frame.up};
        for (int bin = 0; bin < kResolution; ++bin) {
            ring.vertices.append(center + directions.at(bin) * walls.at(bin));
        }
        return ring;
    };
    const QVector3D a = leg.fromPosition;
    const QVector3D b = leg.toPosition;

    cwPassageTubeBuilder::Debug debug;
    debug.legs = {leg};
    cwPassageTubeBuilder::Chain chain;
    chain.legs = {{0, false}};
    chain.rings = {ringAt(a, a, {2.0f, 1.5f, 1.0f, 0.5f}), ringAt(b, b, {2.0f, 1.5f, 1.0f, 0.5f})};
    debug.chains = {chain};
    CHECK(Check::ringIssues(debug).isEmpty());

    SECTION("A right wall 0.5 m past its reading") {
        debug.chains[0].rings[1].vertices[0] = b + QVector3D(0, -2.5f, 0);
        const QVector<Check::Issue> issues = Check::ringIssues(debug);
        REQUIRE(issues.size() == 1);
        CHECK(issues.first().type == Check::WallBeyondReading);
        CHECK(issues.first().position == QVector3D(10, -2.5f, 0));
        CHECK(issues.first().detail.contains(QStringLiteral("0.50 m past station b")));
    }

    SECTION("A wall within the tolerance of its reading") {
        debug.chains[0].rings[1].vertices[1] = b + QVector3D(0, 0, 1.6f);
        CHECK(Check::ringIssues(debug).isEmpty());
    }

    SECTION("An inset ring's walls measured from its station") {
        // 0.5 m down the leg and 0.6 m left of b, as an elbow's middle ring
        // sits toward the bend's inside: its left wall reaches 1.6 m from b.
        const QVector3D inset(9.5f, 0.6f, 0);
        debug.chains[0].rings[1] = ringAt(inset, b, {1.4f, 1.5f, 1.0f, 0.5f});
        const QVector<Check::Issue> issues = Check::ringIssues(debug);
        REQUIRE(issues.size() == 1);
        CHECK(issues.first().type == Check::WallBeyondReading);
        CHECK(issues.first().position == inset + QVector3D(0, 1.0f, 0));
    }

    SECTION("An inset ring grown to hold its center past a short reading") {
        // Centered 1.2 m left of b, past its 1 m left wall: the section is
        // grown to hold the center 0.05 m inside, to 1.25 m.
        const QVector3D inset(10, 1.2f, 0);
        cwPassageTubeBuilder::Ring& ring = debug.chains[0].rings[1];
        ring = ringAt(inset, b, {3.2f, 0.42f, 0.05f, 0.14f});
        CHECK(Check::ringIssues(debug).isEmpty());

        ring.vertices[2] = b + QVector3D(0, 1.5f, 0);
        const QVector<Check::Issue> issues = Check::ringIssues(debug);
        REQUIRE(issues.size() == 1);
        CHECK(issues.first().detail.contains(QStringLiteral("0.25 m past station b")));
    }

    SECTION("Grown rings and rings without a section are left out") {
        debug.chains[0].rings[0].vertices[0] = a + QVector3D(0, -3, 0);
        debug.chains[0].rings[0].isGrown = true;
        debug.chains[0].rings[1].vertices[0] = b + QVector3D(0, -3, 0);
        debug.chains[0].rings[1].section.reset();
        CHECK(Check::ringIssues(debug).isEmpty());
    }
}

TEST_CASE("cwPassageTubeMeshCheck reports LRUD walls off the tube surface", "[PassageTube][cwPassageTubeMeshCheck]")
{
    const cwStationLrud lrud {1.0, 1.0, 1.0, 1.0};
    cwPassageTubeBuilder::Leg leg;
    leg.chunkIndex = 0;
    leg.shotIndex = 0;
    leg.from = QStringLiteral("a");
    leg.to = QStringLiteral("b");
    leg.fromPosition = QVector3D(0, 0, 0);
    leg.toPosition = QVector3D(10, 0, 0);
    leg.fromLrud = lrud;
    leg.toLrud = lrud;
    leg.fromPassageLrud = lrud;
    leg.toPassageLrud = lrud;
    leg.fromFacing = QVector3D(1, 0, 0);
    leg.toFacing = QVector3D(1, 0, 0);

    cwPassageTubeBuilder::Debug debug;
    debug.legs = {leg};

    debug.mesh = boxMesh(QVector3D(0, -1, -1), QVector3D(10, 1, 1));
    CHECK(Check::wallsOnSurface(debug).isEmpty());

    SECTION("A low ceiling misses both ups") {
        debug.mesh = boxMesh(QVector3D(0, -1, -1), QVector3D(10, 1, 0.5f));
        const QVector<Check::Issue> issues = Check::wallsOnSurface(debug);
        REQUIRE(issues.size() == 2);
        CHECK(issues.at(0).type == Check::WallMissed);
        CHECK(issues.at(0).position == QVector3D(0, 0, 1));
        CHECK(issues.at(1).position == QVector3D(10, 0, 1));
    }

    SECTION("A shaft reading the builder replaced is left out") {
        debug.mesh = boxMesh(QVector3D(0, -1, -1), QVector3D(10, 1, 0.3f));
        debug.legs[0].fromPassageLrud.up = 0.5;
        debug.legs[0].toPassageLrud.up = 0.5;
        CHECK(Check::wallsOnSurface(debug).isEmpty());
    }
}

TEST_CASE("cwPassageTubeMeshCheck reports centerline points outside the tube", "[PassageTube][cwPassageTubeMeshCheck]")
{
    cwPassageTubeBuilder::Leg leg;
    leg.from = QStringLiteral("a");
    leg.to = QStringLiteral("b");
    leg.fromPosition = QVector3D(0, 0, 0);
    leg.toPosition = QVector3D(10, 0, 0);

    cwPassageTubeBuilder::Chain chain;
    chain.legs = {cwPassageTubeBuilder::ChainLeg {0, false}};
    chain.rings = {cwPassageTubeBuilder::Ring()};

    cwPassageTubeBuilder::Debug debug;
    debug.legs = {leg};
    debug.chains = {chain};

    SECTION("A tube around the whole leg") {
        const cwPassageTubeMesh mesh = boxMesh(QVector3D(-1, -1, -1), QVector3D(11, 1, 1));
        CHECK(Check::centerlineInside(mesh, debug).isEmpty());
    }

    SECTION("A tube that stops 3 m short of b") {
        const cwPassageTubeMesh mesh = boxMesh(QVector3D(-1, -1, -1), QVector3D(7, 1, 1));
        const QVector<Check::Issue> issues = Check::centerlineInside(mesh, debug);
        REQUIRE(issues.size() == 1);
        CHECK(issues.at(0).type == Check::CenterlineOutside);
        CHECK(issues.at(0).position.x() > 7.0f - static_cast<float>(Check::kCenterlineTolerance));
    }

    SECTION("A station 5 cm outside the tube is reported, 5 mm outside lies on the wall") {
        constexpr float kOutsideMeters = 0.05f;
        constexpr float kOnWallMeters = 0.005f;
        const cwPassageTubeMesh outside = boxMesh(QVector3D(-1, -1, -1), QVector3D(10.0f - kOutsideMeters, 1, 1));
        const QVector<Check::Issue> issues = Check::centerlineInside(outside, debug);
        REQUIRE(issues.size() == 1);
        CHECK(issues.at(0).type == Check::CenterlineOutside);
        CHECK(issues.at(0).position == QVector3D(10, 0, 0));

        const cwPassageTubeMesh onWall = boxMesh(QVector3D(-1, -1, -1), QVector3D(10.0f - kOnWallMeters, 1, 1));
        CHECK(Check::centerlineInside(onWall, debug).isEmpty());
    }

    SECTION("Two overlapping tubes still hold the leg") {
        cwPassageTubeMesh mesh = boxMesh(QVector3D(-1, -1, -1), QVector3D(6, 1, 1));
        appendBox(mesh, QVector3D(4, -1, -1), QVector3D(11, 1, 1));
        CHECK(Check::centerlineInside(mesh, debug).isEmpty());
    }

    SECTION("A leg of an arm left inside its through passage has no rings") {
        debug.chains[0].rings.clear();
        const cwPassageTubeMesh mesh = boxMesh(QVector3D(-1, -1, -1), QVector3D(7, 1, 1));
        CHECK(Check::centerlineInside(mesh, debug).isEmpty());
    }
}
