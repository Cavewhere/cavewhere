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

QVector<QVector3D> mouthOf(const Builder::Chain& arm)
{
    QVector<QVector3D> positions;
    for (const Builder::MouthPoint& point : arm.endMouth.isEmpty() ? arm.startMouth : arm.endMouth) {
        positions.append(point.position);
    }
    return positions;
}

void checkMouthOnSurface(const Builder::Debug& debug, int throughChain, int armChain)
{
    const QVector<QVector3D> mouth = mouthOf(debug.chains.at(armChain));
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

// Random seed 1's one junction: an arm as big as the through meets it
// obliquely, so its mouth spans many more through rings than it has columns.
Builder::Debug buildRandomSeedOne()
{
    const Survey survey = cwPassageTubeCases::toSurvey(cwPassageTubeCases::random(1, 1));
    return buildDebug(survey);
}

// The only window of the build, and the chain that holds it.
struct OnlyWindow {
    const Builder::Chain* through = nullptr;
    const Builder::JunctionWindow* window = nullptr;
};

OnlyWindow onlyWindow(const Builder::Debug& debug)
{
    OnlyWindow found;
    int count = 0;
    for (const Builder::Chain& chain : debug.chains) {
        for (const Builder::JunctionWindow& window : chain.windows) {
            found = OnlyWindow {&chain, &window};
            ++count;
        }
    }
    REQUIRE(count == 1);
    return found;
}

// The through's ring spacing over the rings the window's boundary spans: the
// widest gap between two of them, which densifying holds to the junction's
// spacing.
double windowRingSpacing(const OnlyWindow& found)
{
    int first = std::numeric_limits<int>::max();
    int last = std::numeric_limits<int>::lowest();
    for (const QVector<QPair<int, int>>& boundary : found.window->boundaries) {
        for (const QPair<int, int>& vertex : boundary) {
            first = std::min(first, vertex.first);
            last = std::max(last, vertex.first);
        }
    }
    REQUIRE(last > first);
    const QVector<Builder::Ring>& rings = found.through->rings;
    double widest = 0.0;
    for (int i = first; i < last; ++i) {
        widest = std::max(widest, static_cast<double>(rings.at(i).center.distanceToPoint(rings.at(i + 1).center)));
    }
    return widest;
}

const QVector<Builder::MouthPoint>& windowMouth(const Builder::Debug& debug, const OnlyWindow& found)
{
    REQUIRE(found.window->mouths.size() == 1);
    const Builder::WindowMouth& windowMouth = found.window->mouths.first();
    const Builder::Chain& arm = debug.chains.at(windowMouth.armChain);
    return windowMouth.isArmStart ? arm.startMouth : arm.endMouth;
}

const Builder::Chain& requireArm(const Builder::Debug& debug, int chainIndex)
{
    REQUIRE(debug.chains.size() > chainIndex);
    const Builder::Chain& arm = debug.chains.at(chainIndex);
    CHECK(arm.endCap == Builder::EndCap::Mouth);
    return arm;
}

const cwPassageTubeCases::Case& galleryCase(const QString& prefix)
{
    static const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();
    const auto found = std::find_if(catalog.cbegin(), catalog.cend(), [&](const cwPassageTubeCases::Case& entry) {
        return entry.name.startsWith(prefix);
    });
    REQUIRE(found != catalog.cend());
    return *found;
}

Builder::Debug buildCase(const cwPassageTubeCases::Case& entry)
{
    Builder::Options options;
    options.sampleSpacingMeters = entry.sampleSpacing;
    return buildDebug(cwPassageTubeCases::toSurvey(entry.trip), options);
}

int armWithMouth(const Builder::Debug& debug)
{
    const auto found = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [](const Builder::Chain& chain) {
        return !chain.startMouth.isEmpty() || !chain.endMouth.isEmpty();
    });
    REQUIRE(found != debug.chains.cend());
    return static_cast<int>(std::distance(debug.chains.cbegin(), found));
}

QVector3D casePosition(const cwPassageTubeCases::Case& entry, const QString& name)
{
    for (const QList<Station>& chunk : entry.trip.chunks) {
        for (const Station& station : chunk) {
            if (station.name == name && station.position) {
                return *station.position;
            }
        }
    }
    FAIL("no station " << name.toStdString());
    return {};
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

// The window of an arm that shares no through quads has one boundary loop
// and one mouth, and its zip band starts and runs as it did before windows
// could hold several mouths: each triangle as "ring,column" corners, ring -1
// for a mouth point.
TEST_CASE("cwPassageTubeJunction zips the symmetric T's single mouth as before", "[PassageTube][cwPassageTubeJunction]")
{
    const QString kExpectedZip = QStringLiteral(
        "19,0 19,1 -1,0;-1,0 19,1 -1,1;19,1 18,1 -1,1;-1,1 18,1 -1,2;-1,2 18,1 -1,3;18,1 18,2 -1,3;18,2 17,2 -1,3;"
        "17,2 16,2 -1,3;-1,3 16,2 -1,4;-1,4 16,2 -1,5;16,2 15,2 -1,5;15,2 14,2 -1,5;14,2 14,1 -1,5;-1,5 14,1 -1,6;"
        "-1,6 14,1 -1,7;14,1 13,1 -1,7;13,1 13,0 -1,7;-1,7 13,0 -1,8;-1,8 13,0 -1,9;13,0 13,15 -1,9;13,15 14,15 -1,9;"
        "-1,9 14,15 -1,10;-1,10 14,15 -1,11;14,15 14,14 -1,11;14,14 15,14 -1,11;15,14 16,14 -1,11;-1,11 16,14 -1,12;"
        "-1,12 16,14 -1,13;16,14 17,14 -1,13;17,14 18,14 -1,13;18,14 18,15 -1,13;-1,13 18,15 -1,14;-1,14 18,15 -1,15;"
        "18,15 19,15 -1,15;19,15 19,0 -1,15;-1,15 19,0 -1,0");
    const Trip trip {{throughAlongX(roundSection(1)), armTo(QStringLiteral("d"), QVector3D(10, -10, 0), roundSection(0.5))}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    REQUIRE(debug.chains.at(0).windows.size() == 1);
    const Builder::JunctionWindow& window = debug.chains.at(0).windows.first();
    CHECK(window.mouths.size() == 1);
    CHECK(window.boundaries.size() == 1);

    QStringList triangles;
    for (const std::array<Builder::ZipCorner, 3>& triangle : window.zip) {
        QStringList corners;
        for (const Builder::ZipCorner& corner : triangle) {
            CHECK(corner.mouth == 0);
            corners.append(QStringLiteral("%1,%2").arg(corner.ring).arg(corner.column));
        }
        triangles.append(corners.join(QLatin1Char(' ')));
    }
    CHECK(triangles.join(QLatin1Char(';')).toStdString() == kExpectedZip.toStdString());
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

    for (const QVector3D& point : mouthOf(pit)) {
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
    // The entrance passage runs on around the loop, turning less than a
    // hairpin at each corner, and ends where it joined it.
    const QStringList dimensions = roundSection(0.5);
    const Trip trip {{{station(QStringLiteral("e"), QVector3D(-10, 0, 0), dimensions),
                       station(QStringLiteral("a"), QVector3D(0, 0, 0), dimensions),
                       station(QStringLiteral("b"), QVector3D(6, 6, 0), dimensions),
                       station(QStringLiteral("c"), QVector3D(12, 0, 0), dimensions),
                       station(QStringLiteral("d"), QVector3D(6, -6, 0), dimensions),
                       station(QStringLiteral("a"), QVector3D(0, 0, 0), dimensions)}}};
    const Builder::Debug debug = buildDebug(makeTripsInput({trip}));
    REQUIRE(debug.chains.size() == 1);
    CHECK(debug.chains.at(0).windows.size() == 1);
    CHECK(isClosedSurface(debug.mesh));
}

// Two arms 30 degrees apart on the same side run into each other above the
// through, so their mouths cross on its surface: the second keeps the fan
// cap, hidden inside the through without crossing its wall. Its tube still
// runs through the first arm's mouth and zip band, as the passages do.
TEST_CASE("cwPassageTubeJunction keeps the fallback for arms that run into each other", "[PassageTube][cwPassageTubeJunction]")
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
    REQUIRE(warningCount(debug, Warning::JunctionOverlap) == 1);
    const auto overlap = std::find_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const Warning& warning) {
        return warning.type == Warning::JunctionOverlap;
    });
    CHECK(overlap->message.contains(QStringLiteral("run into each other")));
    REQUIRE(debug.chains.at(0).windows.size() == 1);
    CHECK(debug.chains.at(0).windows.first().mouths.size() == 1);
    const auto isFanCapped = [](const Builder::Chain& chain) { return chain.endCap == Builder::EndCap::Fan; };
    REQUIRE(std::count_if(debug.chains.cbegin() + 1, debug.chains.cend(), isFanCapped) == 1);

    const int capped = static_cast<int>(std::find_if(debug.chains.cbegin() + 1, debug.chains.cend(), isFanCapped) - debug.chains.cbegin());
    const auto isThroughWall = [&](int triangle) {
        return debug.triangleChain.at(triangle) == 0 && !debug.triangleInZipBand.at(triangle);
    };
    const Check::Report report = Check::check(debug.mesh, debug);
    INFO(Check::toText(report).toStdString());
    for (const Check::Issue& issue : report.issues) {
        if (issue.type != Check::SelfIntersection) {
            continue;
        }
        const bool isCappedArm = debug.triangleChain.at(issue.triangle) == capped || debug.triangleChain.at(issue.otherTriangle) == capped;
        CHECK_FALSE((isCappedArm && (isThroughWall(issue.triangle) || isThroughWall(issue.otherTriangle))));
    }
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

TEST_CASE("cwPassageTubeJunction samples the mouth to the through's grid", "[PassageTube][cwPassageTubeJunction]")
{
    const Builder::Debug debug = buildRandomSeedOne();
    const OnlyWindow found = onlyWindow(debug);
    const QVector<Builder::MouthPoint>& mouth = windowMouth(debug, found);
    const int resolution = Builder::Options().ringResolution;
    CHECK(mouth.size() > resolution);
    CHECK(found.window->mouths.first().mouthCorners.size() == mouth.size());
    for (int i = 1; i < mouth.size(); ++i) {
        CHECK(mouth.at(i - 1).column < mouth.at(i).column);
    }
    for (int column = 0; column < resolution; ++column) {
        CHECK(std::any_of(mouth.cbegin(), mouth.cend(), [column](const Builder::MouthPoint& point) {
            return point.column == column;
        }));
    }
}

// Where the arm's columns graze the through's ceiling, each enters on one
// side of the graze or the other, so neighboring columns enter meters apart
// and a column between them lands on either side; splitting the gap there
// would zigzag the mouth, so it stays long.
TEST_CASE("cwPassageTubeJunction keeps every mouth step within twice the through's spacing", "[PassageTube][cwPassageTubeJunction][!mayfail]")
{
    constexpr double kMaxMouthStepSpacings = 2.0;
    const Builder::Debug debug = buildRandomSeedOne();
    const OnlyWindow found = onlyWindow(debug);
    const QVector<Builder::MouthPoint>& mouth = windowMouth(debug, found);
    const double spacing = windowRingSpacing(found);
    for (int i = 0; i < mouth.size(); ++i) {
        const Builder::MouthPoint& point = mouth.at(i);
        const Builder::MouthPoint& next = mouth.at((i + 1) % mouth.size());
        INFO("mouth points at column " << point.column << " and " << next.column << ", spacing " << spacing);
        CHECK(point.position.distanceToPoint(next.position) <= kMaxMouthStepSpacings * spacing);
    }
}

TEST_CASE("cwPassageTubeJunction meshes the mouth strip as a closed fan", "[PassageTube][cwPassageTubeJunction]")
{
    const Builder::Debug debug = buildRandomSeedOne();
    const OnlyWindow found = onlyWindow(debug);
    REQUIRE(windowMouth(debug, found).size() > Builder::Options().ringResolution);
    CHECK(isClosedSurface(debug.mesh));
}

TEST_CASE("cwPassageTubeJunction zips a sparse mouth", "[PassageTube][cwPassageTubeJunction]")
{
    const Builder::Debug debug = buildRandomSeedOne();
    const Check::Report report = Check::check(debug.mesh, debug);
    INFO(Check::toText(report).toStdString());
    for (const Check::Issue& issue : report.issues) {
        CHECK(issue.type != Check::Crease);
        CHECK(issue.type != Check::SliverTriangle);
        CHECK(issue.type != Check::SelfIntersection);
    }
}

TEST_CASE("cwPassageTubeJunction cuts an arm along a room's wall where its axis enters the room", "[PassageTube][cwPassageTubeJunction]")
{
    constexpr double kMaxMouthSpacings = 1.5;
    // Gallery 21's arm reads at most this far from its stations.
    constexpr double kArmLargestReading = 3.5;
    const cwPassageTubeCases::Case& entry = galleryCase(QStringLiteral("21."));
    const Builder::Debug debug = buildCase(entry);
    const Builder::Chain& arm = debug.chains.at(armWithMouth(debug));
    for (const QString& bend : {QStringLiteral("a01"), QStringLiteral("a00")}) {
        const QVector3D corner = casePosition(entry, bend);
        INFO("bend at " << bend.toStdString());
        CHECK(std::any_of(arm.rings.cbegin(), arm.rings.cend(), [&](const Builder::Ring& ring) {
            return ring.isElbow && ring.center.distanceToPoint(corner) <= entry.sampleSpacing;
        }));
    }

    const OnlyWindow found = onlyWindow(debug);
    const int throughIndex = static_cast<int>(found.through - debug.chains.constData());
    const double spacing = windowRingSpacing(found);
    const QVector3D junction = casePosition(entry, QStringLiteral("J"));
    const double wallReach = junction.distanceToPoint(casePosition(entry, QStringLiteral("a00"))) + kArmLargestReading;
    for (const QVector3D& point : mouthOf(arm)) {
        INFO("mouth point (" << point.x() << ", " << point.y() << ", " << point.z() << "), spacing " << spacing);
        CHECK(distanceToTubeSurface(debug, throughIndex, point) <= kMaxMouthSpacings * spacing);
        CHECK(point.distanceToPoint(junction) <= wallReach);
    }
    CHECK(Check::centerlineInside(debug.mesh, debug).isEmpty());
}

TEST_CASE("cwPassageTubeJunction keeps the elbow of an arm bending beside the through", "[PassageTube][cwPassageTubeJunction]")
{
    const cwPassageTubeCases::Case& entry = galleryCase(QStringLiteral("22."));
    const Builder::Debug debug = buildCase(entry);
    const Builder::Chain& arm = debug.chains.at(armWithMouth(debug));
    const auto elbowRings = std::count_if(arm.rings.cbegin(), arm.rings.cend(), [](const Builder::Ring& ring) { return ring.isElbow; });
    INFO("elbow rings " << elbowRings << " of " << arm.rings.size());
    CHECK(elbowRings >= 2);

    const QVector<Check::Issue> issues = Check::centerlineInside(debug.mesh, debug);
    INFO(Check::toText({issues, 0, {}}).toStdString());
    CHECK(issues.isEmpty());
}

// These arms enter their through along a straight run, where the cut at the
// axis keeps the rings the first column's entry kept, so their mouths stay
// where they were before the axis cut.
TEST_CASE("cwPassageTubeJunction keeps the mouths of arms that run straight into the through", "[PassageTube][cwPassageTubeJunction]")
{
    constexpr float kMouthShiftTolerance = 1e-3f;
    struct ExpectedMouth {
        QString casePrefix;
        int armChain = -1;
        int keptRings = 0;
        int mouthSize = 0;
        QVector3D centroid;
    };
    const QVector<ExpectedMouth> expected {
        {QStringLiteral("1. "), 1, 9, 16, QVector3D(0.6399f, 0.0f, 0.1574f)},
        {QStringLiteral("5. "), 1, 6, 25, QVector3D(0.7489f, -1.6127f, 0.1090f)},
        {QStringLiteral("11. "), 1, 10, 16, QVector3D(0.0387f, 0.0f, 0.1572f)},
        {QStringLiteral("Phake Cave"), 1, 2, 22, QVector3D(23.0290f, 22.0490f, -2.9788f)},
        {QStringLiteral("Phake Cave"), 2, 15, 19, QVector3D(31.7972f, 8.9665f, -3.9049f)},
        {QStringLiteral("Phake Cave"), 3, 22, 22, QVector3D(26.8181f, 1.8491f, -30.8821f)},
    };
    // Rows of one case sit together, so each case is built once.
    QString builtPrefix;
    Builder::Debug debug;
    for (const ExpectedMouth& mouth : expected) {
        INFO(mouth.casePrefix.toStdString() << " chain " << mouth.armChain);
        if (mouth.casePrefix != builtPrefix) {
            debug = buildCase(galleryCase(mouth.casePrefix));
            builtPrefix = mouth.casePrefix;
        }
        REQUIRE(debug.chains.size() > mouth.armChain);
        const Builder::Chain& arm = debug.chains.at(mouth.armChain);
        const QVector<QVector3D> points = mouthOf(arm);
        CHECK(arm.rings.size() == mouth.keptRings);
        REQUIRE(points.size() == mouth.mouthSize);
        QVector3D centroid;
        for (const QVector3D& point : points) {
            centroid += point;
        }
        centroid /= static_cast<float>(points.size());
        INFO("centroid (" << centroid.x() << ", " << centroid.y() << ", " << centroid.z() << ")");
        CHECK((centroid - mouth.centroid).length() <= kMouthShiftTolerance);
    }
}
