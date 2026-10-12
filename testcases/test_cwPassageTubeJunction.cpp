/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeCases.h"
#include "cwPassageTubeJunction.h"
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QStringList>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {

using Builder = cwPassageTubeBuilder;
using Check = cwPassageTubeMeshCheck;
using Station = cwPassageTubeCases::Station;
using Trip = cwPassageTubeCases::Trip;
using Survey = cwPassageTubeCases::Survey;
using Warning = cwPassageTubeMesh::Warning;

constexpr double kJunctionTolerance = 0.02;
constexpr double kMouthToleranceMeters = 0.05;
constexpr double kVolumeTolerance = 0.03;
constexpr double kRadiusTolerance = 1e-3;
constexpr double kGrownRadiusTolerance = 0.1;
constexpr double kPi = std::numbers::pi;

Survey makeTripsInput(const QList<Trip>& trips)
{
    return cwPassageTubeCases::toSurvey(trips);
}

Builder::Debug buildDebug(const Survey& survey, const Builder::Options& options = Builder::Options())
{
    return Builder::buildDebug(survey.region, survey.positionsByCave, {}, options);
}

QStringList lrud(double left, double right, double up, double down)
{
    return {QString::number(left), QString::number(right), QString::number(up), QString::number(down)};
}

QStringList roundSection(double radius)
{
    return lrud(radius, radius, radius, radius);
}

Station station(const QString& name, const QVector3D& position, const QStringList& dimensions = QStringList(4))
{
    return Station {name, position, dimensions};
}

// Every trip's tube, and the mesh as a whole, is watertight, consistently
// wound and outward facing.
bool isClosedSurface(const cwPassageTubeMesh& mesh)
{
    QVector<cwPassageTubeMesh::VertexRange> ranges = mesh.tripIndexRanges;
    ranges.append(Check::wholeMesh(mesh));
    return std::all_of(ranges.cbegin(), ranges.cend(), [&](const cwPassageTubeMesh::VertexRange& range) {
        const Check::Report report = Check::closed(mesh, range);
        UNSCOPED_INFO(Check::toText(report).toStdString());
        return report.issues.isEmpty();
    });
}

double signedVolume(const cwPassageTubeMesh& mesh)
{
    return Check::signedVolume(mesh, Check::wholeMesh(mesh));
}

double binAngle(const Builder::Ring& ring, int bin)
{
    return ring.binAngles.at(bin);
}

QVector3D binDirection(const Builder::Ring& ring, int bin)
{
    const double theta = binAngle(ring, bin);
    return ring.frame.right * static_cast<float>(std::cos(theta)) + ring.frame.up * static_cast<float>(std::sin(theta));
}

// The test's own reading of a chain's rings as a solid: the ring whose plane
// lies nearest the point decides, with its radii interpolated by angle.
struct TubeReading {
    int ring = -1;
    double distance = 0.0;
    double radius = 0.0;
};

TubeReading readTube(const Builder::Debug& debug, int chainIndex, const QVector3D& point)
{
    const QVector<Builder::Ring>& rings = debug.chains.at(chainIndex).rings;
    TubeReading reading;
    double nearestAlong = std::numeric_limits<double>::max();
    for (int i = 0; i < rings.size(); ++i) {
        const double along = std::abs(QVector3D::dotProduct(point - rings.at(i).center, rings.at(i).frame.tangent));
        if (along < nearestAlong) {
            nearestAlong = along;
            reading.ring = i;
        }
    }
    const Builder::Ring& ring = rings.at(reading.ring);
    const QVector3D offset = point - ring.center;
    const double a = QVector3D::dotProduct(offset, ring.frame.right);
    const double b = QVector3D::dotProduct(offset, ring.frame.up);
    double theta = std::atan2(b, a);
    if (theta < 0.0) {
        theta += 2.0 * kPi;
    }
    const int resolution = ring.radii.size();
    const double position = theta / (2.0 * kPi) * resolution;
    const int low = static_cast<int>(std::floor(position)) % resolution;
    const double fraction = position - std::floor(position);
    reading.radius = ring.radii.at(low) * (1.0 - fraction) + ring.radii.at((low + 1) % resolution) * fraction;
    reading.distance = std::hypot(a, b);
    return reading;
}

// Inside the chain's tube by more than tolerance.
bool insideTube(const Builder::Debug& debug, int chainIndex, const QVector3D& point, double tolerance = kJunctionTolerance)
{
    const TubeReading reading = readTube(debug, chainIndex, point);
    return reading.distance < reading.radius - tolerance;
}

double distanceToTubeSurface(const Builder::Debug& debug, int chainIndex, const QVector3D& point)
{
    const TubeReading reading = readTube(debug, chainIndex, point);
    return std::abs(reading.distance - reading.radius);
}

const Builder::Ring& ringNearest(const Builder::Chain& chain, const QVector3D& position)
{
    return *std::min_element(chain.rings.cbegin(), chain.rings.cend(), [&](const Builder::Ring& a, const Builder::Ring& b) {
        return a.center.distanceToPoint(position) < b.center.distanceToPoint(position);
    });
}

int warningCount(const Builder::Debug& debug, Warning::Type type)
{
    return static_cast<int>(std::count_if(debug.warnings.cbegin(), debug.warnings.cend(), [type](const Warning& warning) {
        return warning.type == type;
    }));
}

const QVector<QVector3D>& mouthOf(const Builder::Chain& arm)
{
    return arm.endMouth.isEmpty() ? arm.startMouth : arm.endMouth;
}

void checkMouthOnSurface(const Builder::Debug& debug, int throughChain, int armChain)
{
    const QVector<QVector3D>& mouth = mouthOf(debug.chains.at(armChain));
    REQUIRE_FALSE(mouth.isEmpty());
    for (const QVector3D& point : mouth) {
        INFO("mouth point (" << point.x() << ", " << point.y() << ", " << point.z() << ")");
        CHECK(distanceToTubeSurface(debug, throughChain, point) < kMouthToleranceMeters);
    }
}

void checkArmOutsideThrough(const Builder::Debug& debug, int throughChain, int armChain)
{
    const Builder::Chain& arm = debug.chains.at(armChain);
    QVector<QVector3D> vertices = mouthOf(arm);
    for (const Builder::Ring& ring : arm.rings) {
        vertices.append(ring.vertices);
    }
    for (const QVector3D& vertex : std::as_const(vertices)) {
        INFO("arm vertex (" << vertex.x() << ", " << vertex.y() << ", " << vertex.z() << ")");
        CHECK_FALSE(insideTube(debug, throughChain, vertex));
    }
}

// The cross-section area of the ring's polygon: a tube of N bins holds a
// little less than the cylinder through its walls.
double sectionArea(const Builder::Ring& ring)
{
    const int resolution = ring.radii.size();
    double area = 0.0;
    for (int bin = 0; bin < resolution; ++bin) {
        const int next = (bin + 1) % resolution;
        const double angle = binAngle(ring, next) - binAngle(ring, bin) + (next == 0 ? 2.0 * kPi : 0.0);
        area += ring.radii.at(bin) * ring.radii.at(next) * std::sin(angle) / 2.0;
    }
    return area;
}

void checkVolumeNear(const cwPassageTubeMesh& mesh, double expected)
{
    const double volume = signedVolume(mesh);
    INFO("volume " << volume << " m³, expected " << expected << " m³");
    CHECK(std::abs(volume - expected) <= kVolumeTolerance * expected);
}

void checkMeshClean(const Builder::Debug& debug)
{
    const Check::Report report = Check::check(debug.mesh, debug);
    INFO(Check::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

// A straight through passage a → b → c along +x, 20 m long.
QList<Station> throughAlongX(const QStringList& dimensions, const QStringList& junctionDimensions)
{
    return {station(QStringLiteral("a"), QVector3D(0, 0, 0), dimensions),
            station(QStringLiteral("b"), QVector3D(10, 0, 0), junctionDimensions),
            station(QStringLiteral("c"), QVector3D(20, 0, 0), dimensions)};
}

QList<Station> throughAlongX(const QStringList& dimensions)
{
    return throughAlongX(dimensions, dimensions);
}

// An arm chunk from far to b, read with the same dimensions at both ends.
QList<Station> armTo(const QString& name, const QVector3D& far, const QStringList& dimensions)
{
    return {station(name, far, dimensions), station(QStringLiteral("b"), QVector3D(10, 0, 0), dimensions)};
}

const Builder::Chain& requireArm(const Builder::Debug& debug, int chainIndex)
{
    REQUIRE(debug.chains.size() > chainIndex);
    const Builder::Chain& arm = debug.chains.at(chainIndex);
    CHECK(arm.endCap == Builder::EndCap::Mouth);
    return arm;
}

} // namespace

TEST_CASE("cwPassageTubeJunction reads rings as a solid", "[PassageTube][cwPassageTubeJunction]")
{
    const Survey survey = makeTripsInput({Trip {{{station(QStringLiteral("a"), QVector3D(0, 0, 0), roundSection(1)),
                                                  station(QStringLiteral("b"), QVector3D(10, 0, 0), roundSection(1))}}}});
    const Builder::Debug debug = buildDebug(survey);
    REQUIRE(debug.chains.size() == 1);

    cwPassageTubeJunction::RingTube tube {&debug.chains.at(0).rings, QVector3D()};
    CHECK(tube.inside(QVector3D(5, 0, 0)));
    CHECK(tube.inside(QVector3D(5, 0.9f, 0)));
    CHECK_FALSE(tube.inside(QVector3D(5, 1.1f, 0)));
    CHECK(tube.inside(QVector3D(5, 1.01f, 0), kJunctionTolerance));
    CHECK_FALSE(tube.inside(QVector3D(12, 0, 0)));
    CHECK(tube.nearestRing(QVector3D(-1, 0, 0)) == -1);

    const QPointF local = tube.localCoordinates(QVector3D(5, 0, 0.5f), tube.nearestRing(QVector3D(5, 0, 0.5f)));
    CHECK(local.x() == Catch::Approx(0.0).margin(1e-5));
    CHECK(local.y() == Catch::Approx(0.5).margin(1e-5));

    cwPassageTubeJunction::RingTube extended {&debug.chains.at(0).rings, QVector3D(1, 0, 0)};
    CHECK(extended.inside(QVector3D(12, 0, 0)));
    CHECK(extended.nearestRing(QVector3D(12, 0, 0)) == debug.chains.at(0).rings.size() - 1);
}

TEST_CASE("cwPassageTubeJunction merges a symmetric T without interior faces", "[PassageTube][cwPassageTubeJunction]")
{
    const Trip trip {{throughAlongX(roundSection(1)), armTo(QStringLiteral("d"), QVector3D(10, -10, 0), roundSection(0.5))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));

    const Builder::Chain& arm = requireArm(debug, 1);
    CHECK(arm.endMouth.size() == Builder::Options().ringResolution);
    REQUIRE(debug.chains.at(0).windows.size() == 1);
    CHECK(warningCount(debug, Warning::JunctionOverlap) == 0);

    CHECK(isClosedSurface(debug.mesh));
    const double throughSection = sectionArea(debug.chains.at(0).rings.first());
    const double armSection = sectionArea(arm.rings.first());
    checkVolumeNear(debug.mesh, throughSection * 20.0 + armSection * (10.0 - 1.0));
    checkArmOutsideThrough(debug, 0, 1);
    checkMouthOnSurface(debug, 0, 1);

    const Builder::Ring& junction = ringNearest(debug.chains.at(0), QVector3D(10, 0, 0));
    for (double radius : junction.radii) {
        CHECK(radius == Catch::Approx(1.0).margin(kRadiusTolerance));
    }
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeJunction fills a trench to the far wall", "[PassageTube][cwPassageTubeJunction]")
{
    constexpr double kThroughFloor = 0.05;
    constexpr double kArmFloor = 2.5;
    constexpr double kArmHalfWidth = 0.5;
    constexpr double kPastFootprint = 1.5;
    const int downBin = 3 * Builder::Options().ringResolution / 4;

    const Trip trip {{throughAlongX(lrud(1, 1, 1, kThroughFloor)),
                      armTo(QStringLiteral("d"), QVector3D(10, -10, 0), lrud(kArmHalfWidth, kArmHalfWidth, 1, kArmFloor))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    requireArm(debug, 1);

    const Builder::Chain& through = debug.chains.at(0);
    CHECK(ringNearest(through, QVector3D(10, 0, 0)).radii.at(downBin) == Catch::Approx(kArmFloor).margin(kGrownRadiusTolerance));
    for (const Builder::Ring& ring : through.rings) {
        INFO("ring at x " << ring.center.x());
        if (std::abs(ring.center.x() - 10.0f) >= kArmHalfWidth + kPastFootprint) {
            CHECK(ring.radii.at(downBin) == Catch::Approx(kThroughFloor).margin(kRadiusTolerance));
        }
        CHECK(ring.radii.at(downBin) <= kArmFloor + kGrownRadiusTolerance);
    }
    CHECK(isClosedSurface(debug.mesh));
}

TEST_CASE("cwPassageTubeJunction chambers a through passage under a tall arm", "[PassageTube][cwPassageTubeJunction]")
{
    constexpr double kThroughHalfWidth = 1.5;
    constexpr double kArmCeiling = 5.0;
    const int resolution = Builder::Options().ringResolution;
    const int upBin = resolution / 4;
    const int farWallBin = resolution / 2;

    // The arm is as wide as its ceiling allows before it reads as an aven.
    const Trip trip {{throughAlongX(lrud(kThroughHalfWidth, kThroughHalfWidth, 1, 1)),
                      armTo(QStringLiteral("d"), QVector3D(10, -10, 0), lrud(1, 1, kArmCeiling, 1))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    requireArm(debug, 1);
    CHECK(warningCount(debug, Warning::ShaftReading) == 0);

    const Builder::Ring& junction = ringNearest(debug.chains.at(0), QVector3D(10, 0, 0));
    CHECK(junction.radii.at(upBin) == Catch::Approx(kArmCeiling).margin(kGrownRadiusTolerance));
    CHECK(junction.radii.at(farWallBin) == Catch::Approx(kThroughHalfWidth).margin(kRadiusTolerance));
    CHECK(isClosedSurface(debug.mesh));
}

TEST_CASE("cwPassageTubeJunction lands a pit on the passage floor", "[PassageTube][cwPassageTubeJunction]")
{
    constexpr float kCeiling = 1.0f;
    constexpr double kPitDepth = 10.0;
    const int downBin = 3 * Builder::Options().ringResolution / 4;

    const Trip trip {{throughAlongX(lrud(2, 2, kCeiling, 1)),
                      {station(QStringLiteral("p"), QVector3D(10, 0, kPitDepth)), station(QStringLiteral("b"), QVector3D(10, 0, 0))}}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    const Builder::Chain& pit = requireArm(debug, 1);

    for (const QVector3D& point : pit.endMouth) {
        INFO("mouth point (" << point.x() << ", " << point.y() << ", " << point.z() << ")");
        CHECK(std::abs(point.z() - kCeiling) < kMouthToleranceMeters);
    }
    for (const Builder::Ring& ring : debug.chains.at(0).rings) {
        INFO("ring at x " << ring.center.x());
        CHECK(ring.radii.at(downBin) == Catch::Approx(1.0).margin(kRadiusTolerance));
    }
    CHECK(isClosedSurface(debug.mesh));
    const double throughSection = sectionArea(debug.chains.at(0).rings.first());
    const double pitSection = sectionArea(pit.rings.first());
    checkVolumeNear(debug.mesh, throughSection * 20.0 + pitSection * (kPitDepth - kCeiling));
}

TEST_CASE("cwPassageTubeJunction joins a passage to a shaft", "[PassageTube][cwPassageTubeJunction]")
{
    constexpr double kShaftRadius = 1.5;
    constexpr double kArmRadius = 0.8;
    constexpr double kMaxFlattening = 0.2;

    Builder::Options options;
    options.defaultRadiusMeters = kShaftRadius;
    const Trip trip {{{station(QStringLiteral("s0"), QVector3D(10, 0, -10)),
                       station(QStringLiteral("b"), QVector3D(10, 0, 0)),
                       station(QStringLiteral("s1"), QVector3D(10, 0, 10))},
                      armTo(QStringLiteral("d"), QVector3D(10, -10, 0), roundSection(kArmRadius))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}), options);
    requireArm(debug, 1);

    CHECK(isClosedSurface(debug.mesh));
    checkMouthOnSurface(debug, 0, 1);
    for (const Builder::Ring& ring : debug.chains.at(0).rings) {
        if (std::abs(ring.center.z()) > kArmRadius) {
            continue;
        }
        for (int bin = 0; bin < ring.radii.size(); ++bin) {
            INFO("ring at z " << ring.center.z() << ", bin " << bin);
            if (binDirection(ring, bin).y() > 0.0f) {
                CHECK(ring.radii.at(bin) <= kShaftRadius + kMaxFlattening);
            }
        }
    }
}

TEST_CASE("cwPassageTubeJunction merges arms at a four-way junction", "[PassageTube][cwPassageTubeJunction]")
{
    const Trip trip {{throughAlongX(roundSection(1)),
                      armTo(QStringLiteral("d"), QVector3D(10, -10, 0), roundSection(0.5)),
                      armTo(QStringLiteral("e"), QVector3D(10, 10, 0), roundSection(0.5))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    REQUIRE(debug.chains.size() == 3);

    const QVector<Builder::JunctionWindow>& windows = debug.chains.at(0).windows;
    REQUIRE(windows.size() == 2);
    CHECK_FALSE(windows.at(0).removedQuads.intersects(windows.at(1).removedQuads));
    CHECK(isClosedSurface(debug.mesh));
    checkMouthOnSurface(debug, 0, 1);
    checkMouthOnSurface(debug, 0, 2);
    checkMeshClean(debug);
}

TEST_CASE("cwPassageTubeJunction keeps a loop's junction closed", "[PassageTube][cwPassageTubeJunction]")
{
    // The entrance passage runs on around the loop and ends where it joined it.
    const QStringList dimensions = roundSection(0.5);
    const Trip trip {{{station(QStringLiteral("e"), QVector3D(-10, 0, 0), dimensions),
                       station(QStringLiteral("a"), QVector3D(0, 0, 0), dimensions),
                       station(QStringLiteral("b"), QVector3D(10, 5, 0), dimensions),
                       station(QStringLiteral("c"), QVector3D(10, -5, 0), dimensions),
                       station(QStringLiteral("a"), QVector3D(0, 0, 0), dimensions)}}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).windows.size() == 1);
    CHECK(isClosedSurface(debug.mesh));
}

TEST_CASE("cwPassageTubeJunction falls back to a fan cap when two mouths overlap", "[PassageTube][cwPassageTubeJunction]")
{
    constexpr double kSpreadDegrees = 30.0;
    const float spread = static_cast<float>(qDegreesToRadians(kSpreadDegrees));
    const QVector3D second(10.0f + 10.0f * std::sin(spread), -10.0f * std::cos(spread), 0.0f);
    const Trip trip {{throughAlongX(roundSection(1)),
                      armTo(QStringLiteral("d"), QVector3D(10, -10, 0), roundSection(0.8)),
                      armTo(QStringLiteral("e"), second, roundSection(0.8))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    REQUIRE(debug.chains.size() == 3);

    CHECK(isClosedSurface(debug.mesh));
    CHECK(warningCount(debug, Warning::JunctionOverlap) == 1);
    CHECK(debug.chains.at(0).windows.size() == 1);
}

TEST_CASE("cwPassageTubeJunction drops an arm that lies inside the through", "[PassageTube][cwPassageTubeJunction]")
{
    const Trip trip {{throughAlongX(roundSection(2)), armTo(QStringLiteral("d"), QVector3D(10, -0.2f, 0), roundSection(0.2))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    REQUIRE(debug.chains.size() == 2);

    CHECK(isClosedSurface(debug.mesh));
    CHECK(warningCount(debug, Warning::ArmInsideThrough) == 1);
    CHECK(debug.chains.at(1).rings.isEmpty());
    CHECK(debug.chains.at(0).windows.isEmpty());
    REQUIRE(debug.mesh.tripIndexRanges.size() == 1);
    checkMeshClean(debug);
}
