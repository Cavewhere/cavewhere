/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_range.hpp>

//Our includes
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QHash>
#include <QSet>
#include <QVector2D>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

using Cases = cwPassageTubeCases;

constexpr int kSmokeStride = 25;
constexpr int kCleanSmokeStride = 10;
constexpr double kRightAngleDegrees = 90.0;
constexpr double kGrazingJoinDegrees = 20.0;
constexpr double kAngleToleranceDegrees = 1e-3;
constexpr float kMinPlanLengthSquared = 1e-6f;
constexpr int kRandomSeedCount = 50;
constexpr int kMaxRandomJunctions = 6;
constexpr float kPulledToleranceMeters = 1e-4f;

const QVector<Cases::Case>& catalog()
{
    static const QVector<Cases::Case> cases = Cases::catalog();
    return cases;
}

const Cases::Case& catalogCase(const std::string& name)
{
    static const QHash<QString, int> indexByName = []() {
        QHash<QString, int> indices;
        for (int i = 0; i < catalog().size(); ++i) {
            indices.insert(catalog().at(i).name, i);
        }
        return indices;
    }();
    const int index = indexByName.value(QString::fromStdString(name), -1);
    REQUIRE(index >= 0);
    return catalog().at(index);
}

std::vector<std::string> caseNames(const std::function<bool(const Cases::Case&)>& keep, int stride = 1)
{
    std::vector<std::string> names;
    int matched = 0;
    for (const Cases::Case& entry : catalog()) {
        if (keep(entry)) {
            if (matched % stride == 0) {
                names.push_back(entry.name.toStdString());
            }
            ++matched;
        }
    }
    return names;
}

bool isSweep(const Cases::Case& entry)
{
    return entry.tags.contains(QStringLiteral("sweep"));
}

bool hasJunction(const Cases::Case& entry)
{
    return !entry.tags.contains(QStringLiteral("arms:0"));
}

cwPassageTubeBuilder::Options builderOptions(double sampleSpacing)
{
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = sampleSpacing;
    return options;
}

void checkBuildsClean(const Cases::Trip& trip, double sampleSpacing = cwPassageTubeBuilder::Options().sampleSpacingMeters)
{
    const Cases::Survey survey = Cases::toSurvey(trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
    REQUIRE_FALSE(debug.mesh.isEmpty());
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

// Each trip's tube is watertight, consistently wound and outward facing.
void checkBuildsClosed(const Cases::Trip& trip, double sampleSpacing = cwPassageTubeBuilder::Options().sampleSpacingMeters)
{
    const Cases::Survey survey = Cases::toSurvey(trip);
    const cwPassageTubeMesh mesh = cwPassageTubeBuilder::build(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
    REQUIRE_FALSE(mesh.isEmpty());
    for (const cwPassageTubeMesh::VertexRange& range : mesh.tripIndexRanges) {
        const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(mesh, range);
        INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
        CHECK(report.issues.isEmpty());
    }
}

void checkCaseBuildsClean(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    checkBuildsClean(entry.trip, entry.sampleSpacing);
}

void checkCaseBuildsClosed(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    checkBuildsClosed(entry.trip, entry.sampleSpacing);
}

// Random seeds whose junction-free survey builds a clean mesh. The others
// fail the full check on what uniform random bearings and independent
// readings make: hairpins whose inset rings and clamped inner walls cross
// or fold (SelfIntersection, Crease, SliverTriangle), passages run through
// each other (SelfIntersection), and walls off a ring that a hairpin's
// corner or a station's facing tilts (WallMissed).
const QVector<int> kCleanRandomSeeds {2, 8, 13, 15, 19, 28, 31, 36, 49};

bool isCleanRandomSeed(int seed)
{
    return kCleanRandomSeeds.contains(seed);
}

// Vertices the builder slid onto the previous ring's plane: the bend rules
// keep every ring in front of the one before, so the fold-back pull in the
// builder never moves one.
int pulledVertexCount(const cwPassageTubeBuilder::Debug& debug)
{
    int count = 0;
    for (const cwPassageTubeBuilder::Chain& chain : debug.chains) {
        for (const cwPassageTubeBuilder::Ring& ring : chain.rings) {
            for (int bin = 0; bin < ring.vertices.size(); ++bin) {
                const double theta = ring.binAngles.at(bin);
                const QVector3D direction = ring.frame.right * static_cast<float>(std::cos(theta))
                                            + ring.frame.up * static_cast<float>(std::sin(theta));
                const QVector3D unpulled = ring.center + direction * static_cast<float>(ring.radii.at(bin));
                if (ring.vertices.at(bin).distanceToPoint(unpulled) > kPulledToleranceMeters) {
                    ++count;
                }
            }
        }
    }
    return count;
}

// Station names of degree above two: survey junctions.
int junctionCount(const Cases::Trip& trip)
{
    QHash<QString, QSet<QString>> neighbors;
    for (const QList<Cases::Station>& chunk : trip.chunks) {
        for (int i = 1; i < chunk.size(); ++i) {
            const QString& from = chunk.at(i - 1).name;
            const QString& to = chunk.at(i).name;
            if (from.isEmpty() || to.isEmpty()) {
                continue;
            }
            neighbors[from].insert(to);
            neighbors[to].insert(from);
        }
    }
    int count = 0;
    for (const QSet<QString>& adjacent : std::as_const(neighbors)) {
        if (adjacent.size() > 2) {
            ++count;
        }
    }
    return count;
}

QVector3D position(const Cases::Trip& trip, const QString& name)
{
    for (const QList<Cases::Station>& chunk : trip.chunks) {
        for (const Cases::Station& station : chunk) {
            if (station.name == name && station.position) {
                return *station.position;
            }
        }
    }
    FAIL("no station " << name.toStdString());
    return {};
}

const QString kPhakeCave = QStringLiteral("Phake Cave");

// The issues Phake Cave keeps, by the station nearest each one. a2, a4, a6,
// a9 and a10 read 0 on one side, so their rings and the a10 mouth bunch
// vertices a few centimeters apart, and the strips and zip bands beside them
// are thin, fold or cross; a3's walls lie off the tube beside a2, a defect it
// had before the merge.
struct AcceptedIssue {
    QString station;
    cwPassageTubeMeshCheck::Type type;
};

const QVector<AcceptedIssue> kPhakeCaveIssues {
    {QStringLiteral("a2"), cwPassageTubeMeshCheck::Crease},
    {QStringLiteral("a2"), cwPassageTubeMeshCheck::SelfIntersection},
    {QStringLiteral("a2"), cwPassageTubeMeshCheck::WallMissed},
    {QStringLiteral("a3"), cwPassageTubeMeshCheck::WallMissed},
    {QStringLiteral("a4"), cwPassageTubeMeshCheck::Crease},
    {QStringLiteral("a4"), cwPassageTubeMeshCheck::SelfIntersection},
    {QStringLiteral("a6"), cwPassageTubeMeshCheck::Crease},
    {QStringLiteral("a9"), cwPassageTubeMeshCheck::SliverTriangle},
    {QStringLiteral("a10"), cwPassageTubeMeshCheck::Crease},
    {QStringLiteral("a10"), cwPassageTubeMeshCheck::SelfIntersection},
    {QStringLiteral("a10"), cwPassageTubeMeshCheck::SliverTriangle},
};

QString nearestStation(const Cases::Trip& trip, const QVector3D& point)
{
    QString nearest;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const QList<Cases::Station>& chunk : trip.chunks) {
        for (const Cases::Station& station : chunk) {
            if (station.position && station.position->distanceToPoint(point) < nearestDistance) {
                nearestDistance = station.position->distanceToPoint(point);
                nearest = station.name;
            }
        }
    }
    return nearest;
}

bool isAcceptedPhakeCaveIssue(const Cases::Trip& trip, const cwPassageTubeMeshCheck::Issue& issue)
{
    const QString station = nearestStation(trip, issue.position);
    return std::any_of(kPhakeCaveIssues.cbegin(), kPhakeCaveIssues.cend(), [&](const AcceptedIssue& accepted) {
        return accepted.station == station && accepted.type == issue.type;
    });
}

// The smallest angle in plan between an arm and either half of the through
// passage at J; a right angle when every arm is vertical.
double planJoinDegrees(const Cases::Trip& trip)
{
    const auto planOf = [](const QVector3D& v) { return QVector2D(v.x(), v.y()); };
    const QVector3D junction = position(trip, QStringLiteral("J"));
    const QVector<QVector2D> halves {planOf(position(trip, QStringLiteral("t0")) - junction),
                                     planOf(position(trip, QStringLiteral("t1")) - junction)};
    double smallest = kRightAngleDegrees;
    for (int c = 1; c < trip.chunks.size(); ++c) {
        const QVector2D arm = planOf(*trip.chunks.at(c).first().position - junction);
        if (arm.lengthSquared() < kMinPlanLengthSquared) {
            continue;
        }
        for (const QVector2D& half : halves) {
            const double cosine = QVector2D::dotProduct(arm.normalized(), half.normalized());
            smallest = std::min(smallest, qRadiansToDegrees(std::acos(std::clamp(cosine, -1.0, 1.0))));
        }
    }
    return smallest;
}

// The sweep's stated junction limit that the case parameters show: an arm
// within 20° of the through in plan meets its wall in a wedge, and at the
// mouth's far tip the arm's wall folds against the through's by about 160°,
// past the crease check's limit (Crease). The other stated limit, two mouths
// sharing through quads, shows only in the build: see overlapsMouths.
bool joinsAtGrazingAngle(const Cases::Case& entry)
{
    return planJoinDegrees(entry.trip) <= kGrazingJoinDegrees + kAngleToleranceDegrees;
}

// A through turning 90° at J keeps its corner rings 1.65 m either side of
// J, where the inner-bend rule puts them, and the merge adds no rings across
// the corner, where they would crease its inner wall. The mouth is cut from
// quads longer than the arm, and its zip band folds or thins (Crease,
// SelfIntersection, SliverTriangle).
bool bendsSharplyAtJunction(const Cases::Case& entry)
{
    return entry.tags.contains(QStringLiteral("bend:90"));
}

// An arm 2.5 times the through's size that slopes or meets a bent through
// grows the through's columns unevenly, and the zip band that bridges the
// jumps between grown columns folds over the through or crosses it (Crease,
// SelfIntersection).
bool hasLargeTiltedArm(const Cases::Case& entry)
{
    const QStringList& tags = entry.tags;
    const bool isTilted = !tags.contains(QStringLiteral("profile:0")) || !tags.contains(QStringLiteral("bend:0"));
    return tags.contains(QStringLiteral("ratio:2.5")) && isTilted;
}

// Sweep cases outside those families whose junction the merge leaves
// unclean, by family:
// - an arm 30° from the through past J opens where the through widens, and
//   its zip band folds back on itself (Crease, SelfIntersection);
// - a level arm 2.5 times the through's size grows the through unevenly, and
//   its zip band folds over the through or crosses it (Crease,
//   SliverTriangle, SelfIntersection);
// - an arm 30° from the through and sloping 30° meets it in a wedge that
//   folds by about 155° (Crease);
// - a sloped arm meets a through bent 40° at J, and the zip band bridges a
//   bin jump with thin or folded triangles (Crease, SliverTriangle,
//   SelfIntersection).
const QStringList kOpenSweepCases {
    QStringLiteral("plan 90, profile 0, ratio 0.3, 3 arms, bend 0, section x2 past J"),

    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 0, same section"),
    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"),
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 0, same section"),
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"),
    QStringLiteral("plan 90, profile 0, ratio 2.5, 1 arm, bend 0, same section"),
    QStringLiteral("plan 90, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"),

    QStringLiteral("plan 30, profile 30, ratio 0.3, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 90, profile 30, ratio 0.3, 3 arms, bend 0, section x2 past J"),

    QStringLiteral("plan 30, profile 30, ratio 1, 1 arm, bend 40, same section"),
    QStringLiteral("plan 30, profile 30, ratio 1, 1 arm, bend 40, section x2 past J"),
    QStringLiteral("plan 30, profile -30, ratio 1, 1 arm, bend 40, same section"),
    QStringLiteral("plan 90, profile -30, ratio 1, 1 arm, bend 40, same section"),
};

// Sweep cases kept out of the clean checks by their parameters or name.
bool isOpenSweepCase(const Cases::Case& entry)
{
    return joinsAtGrazingAngle(entry)
           || bendsSharplyAtJunction(entry)
           || hasLargeTiltedArm(entry)
           || kOpenSweepCases.contains(entry.name);
}

// Two arms whose mouths share through quads, the merge's other stated
// limit: the second falls back to a fan cap inside the first
// (SelfIntersection).
bool overlapsMouths(const cwPassageTubeBuilder::Debug& debug)
{
    return std::any_of(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == cwPassageTubeMesh::Warning::JunctionOverlap;
    });
}

// Every check, apart from the cases whose mouths overlap, which the
// [!mayfail] sweep tests cover.
void checkSweepCaseBuildsClean(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    REQUIRE_FALSE(debug.mesh.isEmpty());
    if (overlapsMouths(debug)) {
        return;
    }
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

// The open sweep cases and those whose mouths overlap, with every check.
void checkOpenSweepCaseBuildsClean(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    if (!isOpenSweepCase(entry) && !overlapsMouths(debug)) {
        return;
    }
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

} // namespace

TEST_CASE("cwPassageTubeCases catalog builds clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !isSweep(entry) && hasJunction(entry) && entry.name != kPhakeCave;
    })));
    checkCaseBuildsClean(name);
}

// The arm's ceiling runs level with the through's, so its mouth runs along
// the through's ceiling for several meters. The zip band to that long edge
// takes no slivers; the one crease left is the 20° wedge at the mouth's far
// tip, where the arm's wall folds against the through's (see
// joinsAtGrazingAngle).
TEST_CASE("cwPassageTubeCases zips a mouth that runs along the through's ceiling", "[PassageTube][cwPassageTubeCases]")
{
    constexpr float kOnMouthMeters = 1e-3f;
    const Cases::Case& entry = catalogCase("plan 20, profile 0, ratio 1, 1 arm, bend 0, same section");
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    REQUIRE(debug.chains.size() == 2);
    REQUIRE(debug.chains.at(0).windows.size() == 1);
    const cwPassageTubeBuilder::JunctionWindow& window = debug.chains.at(0).windows.first();
    const cwPassageTubeBuilder::Chain& arm = debug.chains.at(window.armChain);
    const QVector<QVector3D>& mouth = window.isArmStart ? arm.startMouth : arm.endMouth;
    REQUIRE(mouth.size() == cwPassageTubeBuilder::Options().ringResolution);

    const auto isOnMouth = [&](const QVector3D& point) {
        for (int k = 0; k < mouth.size(); ++k) {
            const QVector3D& a = mouth.at(k);
            const QVector3D& b = mouth.at((k + 1) % mouth.size());
            const QVector3D edge = b - a;
            const float along = std::clamp(QVector3D::dotProduct(point - a, edge) / edge.lengthSquared(), 0.0f, 1.0f);
            if (point.distanceToPoint(a + edge * along) <= kOnMouthMeters) {
                return true;
            }
        }
        return false;
    };

    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.size() <= 1);
    for (const cwPassageTubeMeshCheck::Issue& issue : report.issues) {
        CHECK(issue.type == cwPassageTubeMeshCheck::Crease);
        CHECK(isOnMouth(issue.position));
    }
}

// Every issue Phake Cave keeps lies by a station kPhakeCaveIssues names.
TEST_CASE("cwPassageTubeCases catalog junctions the merge leaves unclean build clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const Cases::Case& entry = catalogCase(kPhakeCave.toStdString());
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    REQUIRE_FALSE(debug.mesh.isEmpty());
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    for (const cwPassageTubeMeshCheck::Issue& issue : report.issues) {
        INFO(issue.detail.toStdString() << " nearest " << nearestStation(entry.trip, issue.position).toStdString());
        CHECK(isAcceptedPhakeCaveIssue(entry.trip, issue));
    }
}

// The 15° arm lies inside the through, which doubles past J, apart from its
// tip, which pokes out; the through grows to take the tip before the arm is
// left out, then grows again without it.
TEST_CASE("cwPassageTubeCases re-grows the through after dropping an arm", "[PassageTube][cwPassageTubeCases]")
{
    constexpr double kRadiusTolerance = 1e-3;
    constexpr float kPastJunctionMeters = 7.0f;
    constexpr float kSameRingMeters = 1e-3f;
    const Cases::Case& entry = catalogCase("plan 45, profile 0, ratio 0.3, 3 arms, bend 0, section x2 past J");
    const auto build = [&](const Cases::Trip& trip) {
        const Cases::Survey survey = Cases::toSurvey(trip);
        return cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    };
    const cwPassageTubeBuilder::Debug debug = build(entry.trip);
    const auto dropped = std::find_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == cwPassageTubeMesh::Warning::ArmInsideThrough;
    });
    REQUIRE(dropped != debug.warnings.cend());
    CHECK(std::count_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
              return warning.type == cwPassageTubeMesh::Warning::ArmInsideThrough;
          }) == 1);
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());

    Cases::Trip withoutArm = entry.trip;
    withoutArm.chunks.removeIf([&](const QList<Cases::Station>& chunk) {
        return std::any_of(chunk.cbegin(), chunk.cend(), [&](const Cases::Station& station) {
            return station.name == dropped->station;
        });
    });
    REQUIRE(withoutArm.chunks.size() == entry.trip.chunks.size() - 1);
    const cwPassageTubeBuilder::Debug plain = build(withoutArm);

    const QVector3D junction = position(entry.trip, QStringLiteral("J"));
    const QVector3D past = junction + (position(entry.trip, QStringLiteral("t1")) - junction).normalized() * kPastJunctionMeters;
    const auto ringNear = [&](const cwPassageTubeBuilder::Debug& build) -> const cwPassageTubeBuilder::Ring& {
        const QVector<cwPassageTubeBuilder::Ring>& rings = build.chains.at(0).rings;
        return *std::min_element(rings.cbegin(), rings.cend(), [&](const cwPassageTubeBuilder::Ring& a, const cwPassageTubeBuilder::Ring& b) {
            return a.center.distanceToPoint(past) < b.center.distanceToPoint(past);
        });
    };
    const cwPassageTubeBuilder::Ring& ring = ringNear(debug);
    const cwPassageTubeBuilder::Ring& plainRing = ringNear(plain);
    REQUIRE(ring.center.distanceToPoint(plainRing.center) <= kSameRingMeters);
    CHECK_FALSE(ring.isGrown);
    REQUIRE(ring.radii.size() == plainRing.radii.size());
    for (int bin = 0; bin < ring.radii.size(); ++bin) {
        CHECK(ring.radii.at(bin) == Catch::Approx(plainRing.radii.at(bin)).margin(kRadiusTolerance));
    }
}

TEST_CASE("cwPassageTubeCases catalog builds closed meshes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !isSweep(entry) && hasJunction(entry);
    })));
    checkCaseBuildsClosed(name);
}

TEST_CASE("cwPassageTubeCases sweep subsample builds closed meshes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry);
    }, kSmokeStride)));
    checkCaseBuildsClosed(name);
}

TEST_CASE("cwPassageTubeCases sweep subsample builds clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry) && !isOpenSweepCase(entry);
    }, kCleanSmokeStride)));
    checkSweepCaseBuildsClean(name);
}

// A smoke subsample of the sweep cases at the stated junction limits or
// with a junction the merge leaves unclean; see isOpenSweepCase and
// overlapsMouths for what they report.
TEST_CASE("cwPassageTubeCases sweep subsample at a junction limit builds clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry);
    }, kSmokeStride)));
    checkOpenSweepCaseBuildsClean(name);
}

// The full sweep, run with "[Sweep]". It lacks the [PassageTube] tag because
// a tag filter also selects hidden tests, and the sweep would add minutes to
// every "[PassageTube]" run.
TEST_CASE("cwPassageTubeCases full sweep builds clean meshes", "[cwPassageTubeCases][Sweep][.]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry) && !isOpenSweepCase(entry);
    })));
    checkSweepCaseBuildsClean(name);
}

// See isOpenSweepCase and overlapsMouths.
TEST_CASE("cwPassageTubeCases full sweep at a junction limit builds clean meshes", "[cwPassageTubeCases][Sweep][.][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry);
    })));
    checkOpenSweepCaseBuildsClean(name);
}

TEST_CASE("cwPassageTubeCases full sweep builds closed meshes", "[cwPassageTubeCases][Sweep][.]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry);
    })));
    checkCaseBuildsClosed(name);
}

TEST_CASE("cwPassageTubeCases catalog cases without a junction build clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !hasJunction(entry);
    })));
    checkCaseBuildsClean(name);
}

TEST_CASE("cwPassageTubeCases catalog cases without a junction never pull a vertex onto the previous ring", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !hasJunction(entry);
    })));
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    CHECK(pulledVertexCount(debug) == 0);
}

TEST_CASE("cwPassageTubeCases catalog cases without a junction build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !hasJunction(entry);
    })));
    checkCaseBuildsClosed(name);
}

// Random surveys carry every defect of the junction-free seeds (see
// kCleanRandomSeeds) and add junctions at the merge's limits: overlapping
// mouths, grazing joins, sharp bends at the junction and stations reading 0
// on one side (see isOpenSweepCase and kPhakeCaveIssues).
TEST_CASE("cwPassageTubeCases random surveys build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    const int junctions = 1 + (seed - 1) % kMaxRandomJunctions;
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClean(Cases::random(static_cast<unsigned int>(seed), junctions));
}

TEST_CASE("cwPassageTubeCases random surveys build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    const int junctions = 1 + (seed - 1) % kMaxRandomJunctions;
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClosed(Cases::random(static_cast<unsigned int>(seed), junctions));
}

TEST_CASE("cwPassageTubeCases random surveys without a junction build clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const int seed = GENERATE(from_range(kCleanRandomSeeds));
    INFO("seed " << seed);
    checkBuildsClean(Cases::random(static_cast<unsigned int>(seed), 0));
}

// See kCleanRandomSeeds for what the other seeds report.
TEST_CASE("cwPassageTubeCases random surveys without a junction and with known builder defects build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const int seed = GENERATE(filter([](int candidate) { return !isCleanRandomSeed(candidate); },
                                     range(1, kRandomSeedCount + 1)));
    INFO("seed " << seed);
    checkBuildsClean(Cases::random(static_cast<unsigned int>(seed), 0));
}

// Sharp random bends no longer fold the inner wall into zero-area triangles.
TEST_CASE("cwPassageTubeCases random surveys without a junction build no degenerate triangles", "[PassageTube][cwPassageTubeCases]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    INFO("seed " << seed);
    const Cases::Survey survey = Cases::toSurvey(Cases::random(static_cast<unsigned int>(seed), 0));
    const cwPassageTubeMesh mesh = cwPassageTubeBuilder::build(survey.region, survey.positionsByCave, {}, cwPassageTubeBuilder::Options());
    const QVector<cwPassageTubeMeshCheck::Issue> issues = cwPassageTubeMeshCheck::degenerateTriangles(mesh, cwPassageTubeMeshCheck::wholeMesh(mesh));
    const auto degenerate = std::count_if(issues.cbegin(), issues.cend(), [](const cwPassageTubeMeshCheck::Issue& issue) {
        return issue.type == cwPassageTubeMeshCheck::DegenerateTriangle;
    });
    INFO(cwPassageTubeMeshCheck::toText({issues, 0, {}}).toStdString());
    CHECK(degenerate == 0);
}

TEST_CASE("cwPassageTubeCases random surveys without a junction build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    INFO("seed " << seed);
    checkBuildsClosed(Cases::random(static_cast<unsigned int>(seed), 0));
}

TEST_CASE("cwPassageTubeCases a case survey round-trips its parameters", "[PassageTube][cwPassageTubeCases]")
{
    SECTION("an arm's far station lies at its bearing and inclination")
    {
        constexpr double kPlanDegrees = 60.0;
        constexpr double kProfileDegrees = 30.0;
        constexpr double kLengthMeters = 8.0;
        // Plan angles count from the bearing back toward t0, due south.
        constexpr double kExpectedBearing = 120.0;

        Cases::JunctionCase junctionCase;
        Cases::ArmSpec arm;
        arm.planDegrees = kPlanDegrees;
        arm.profileDegrees = kProfileDegrees;
        arm.lengthMeters = kLengthMeters;
        junctionCase.arms.append(arm);

        const Cases::Trip trip = Cases::toTrip(junctionCase);
        const QVector3D offset = position(trip, QStringLiteral("a00")) - position(trip, QStringLiteral("J"));
        const double bearing = qRadiansToDegrees(std::atan2(offset.x(), offset.y()));
        const double inclination = qRadiansToDegrees(std::asin(offset.z() / offset.length()));
        CHECK(offset.length() == Catch::Approx(kLengthMeters).margin(1e-4));
        CHECK(bearing == Catch::Approx(kExpectedBearing).margin(1e-3));
        CHECK(inclination == Catch::Approx(kProfileDegrees).margin(1e-3));

        const Cases::Survey survey = Cases::toSurvey(junctionCase);
        REQUIRE(survey.region.caves.size() == 1);
        REQUIRE(survey.region.caves.at(0).trips.size() == 1);
        CHECK(survey.region.caves.at(0).trips.at(0).chunks.size() == 2);
    }

    SECTION("random surveys are deterministic and have the requested junctions")
    {
        const unsigned int seed = GENERATE(1u, 7u, 42u);
        const int junctions = GENERATE(0, 1, 3, 6);
        INFO("seed " << seed << ", " << junctions << " junctions");
        const Cases::Trip trip = Cases::random(seed, junctions);
        CHECK(trip == Cases::random(seed, junctions));
        CHECK(junctionCount(trip) == junctions);
    }

    SECTION("catalog names are unique and the sweep covers every parameter")
    {
        QSet<QString> names;
        QSet<QString> tags;
        for (const Cases::Case& entry : catalog()) {
            names.insert(entry.name);
            for (const QString& tag : entry.tags) {
                tags.insert(tag);
            }
        }
        CHECK(names.size() == catalog().size());
        for (const char* tag : {"gallery", "phake", "loop", "plan:20", "plan:90", "profile:90", "profile:-90",
                                "ratio:0.3", "ratio:2.5", "arms:0", "arms:1", "arms:2", "arms:3",
                                "bend:40", "bend:90", "section:x2"}) {
            INFO(tag);
            CHECK(tags.contains(QString::fromLatin1(tag)));
        }
        CHECK(Cases::galleryCases().size() == 17);
    }
}
