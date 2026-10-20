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
#include "cwPassageTubeUnion.h"

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
#include <utility>
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
// Readings print with six significant digits.
constexpr double kLrudStepTolerance = 1e-4;
// A station's readings in Cases::Station::lrud order.
constexpr int kLeftReading = 0;
constexpr int kRightReading = 1;
constexpr int kUpReading = 2;
constexpr int kDownReading = 3;

QVector<QVector3D> mouthPositions(const QVector<cwPassageTubeBuilder::MouthPoint>& mouth)
{
    QVector<QVector3D> positions;
    positions.reserve(mouth.size());
    for (const cwPassageTubeBuilder::MouthPoint& point : mouth) {
        positions.append(point.position);
    }
    return positions;
}

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

// "Crease: 4, FoldBack: 2", one entry per issue type in order of first
// appearance.
std::string issueCounts(const QVector<cwPassageTubeMeshCheck::Issue>& issues)
{
    QVector<cwPassageTubeMeshCheck::Type> types;
    QHash<cwPassageTubeMeshCheck::Type, int> counts;
    for (const cwPassageTubeMeshCheck::Issue& issue : issues) {
        if (!counts.contains(issue.type)) {
            types.append(issue.type);
        }
        ++counts[issue.type];
    }
    QStringList entries;
    for (const cwPassageTubeMeshCheck::Type type : std::as_const(types)) {
        entries.append(QStringLiteral("%1: %2").arg(cwPassageTubeMeshCheck::typeName(type)).arg(counts.value(type)));
    }
    return entries.join(QStringLiteral(", ")).toStdString();
}

void checkBuildsClean(const Cases::Survey& survey, double sampleSpacing = cwPassageTubeBuilder::Options().sampleSpacingMeters)
{
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
    REQUIRE_FALSE(debug.mesh.isEmpty());
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO("issue counts: " << issueCounts(report.issues));
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

void checkBuildsClean(const Cases::Trip& trip)
{
    checkBuildsClean(Cases::toSurvey(trip));
}

// The tube drawn with every trip visible is watertight, consistently wound
// and outward facing.
void checkBuildsClosed(const Cases::Survey& survey, double sampleSpacing = cwPassageTubeBuilder::Options().sampleSpacingMeters)
{
    const cwPassageTubeMesh mesh = cwPassageTubeBuilder::build(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
    REQUIRE_FALSE(mesh.isEmpty());
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::closed(mesh, cwPassageTubeMeshCheck::allTripsVisible(mesh));
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

void checkBuildsClosed(const Cases::Trip& trip)
{
    checkBuildsClosed(Cases::toSurvey(trip));
}

void checkCaseBuildsClean(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    checkBuildsClean(Cases::toSurvey(entry), entry.sampleSpacing);
}

void checkCaseBuildsClosed(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    checkBuildsClosed(Cases::toSurvey(entry), entry.sampleSpacing);
}

// Vertices the builder slid level with the previous ring's: a loft keeps
// every ring in front of the one before, so the fold-back pull in the
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
                const QVector3D unpulled = ring.center + direction * static_cast<float>(ring.radii.at(bin))
                                           + ring.frame.tangent * static_cast<float>(ring.offsetsAlong.at(bin));
                if (ring.vertices.at(bin).distanceToPoint(unpulled) > kPulledToleranceMeters) {
                    ++count;
                }
            }
        }
    }
    return count;
}

double angleBetweenDegrees(const QVector3D& a, const QVector3D& b)
{
    const double cosine = QVector3D::dotProduct(a.normalized(), b.normalized());
    return qRadiansToDegrees(std::acos(std::clamp(cosine, -1.0, 1.0)));
}

double distanceToLeg(const QVector3D& point, const QVector3D& start, const QVector3D& end)
{
    const QVector3D along = end - start;
    const float t = std::clamp(QVector3D::dotProduct(point - start, along) / along.lengthSquared(), 0.0f, 1.0f);
    return point.distanceToPoint(start + t * along);
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

// The issues Phake Cave keeps, by the station nearest each one.
struct AcceptedIssue {
    QString station;
    cwPassageTubeMeshCheck::Type type;
};

const QVector<AcceptedIssue> kPhakeCaveIssues {
    // The checker looks for a3's side walls square to the bisector of its
    // legs; they lie along its facing (T07ah4).
    {QStringLiteral("a3"), cwPassageTubeMeshCheck::WallMissed},
    // a4 holds its two chunks' sections, so its tube splits there, and the
    // union folds and leaves slivers where the two overlap.
    {QStringLiteral("a4"), cwPassageTubeMeshCheck::Crease},
    {QStringLiteral("a4"), cwPassageTubeMeshCheck::DegenerateTriangle},
    {QStringLiteral("a4"), cwPassageTubeMeshCheck::SliverTriangle},
    // a6 and a9 hold a plumb and a level section each, until T07am stands
    // their rings vertical: the union leaves slivers where they meet, and
    // a6's rings fold behind the plumb one.
    {QStringLiteral("a6"), cwPassageTubeMeshCheck::FoldBack},
    {QStringLiteral("a6"), cwPassageTubeMeshCheck::SliverTriangle},
    {QStringLiteral("a9"), cwPassageTubeMeshCheck::SliverTriangle},
    // a10 reads 0 on one side, so its ring bunches vertices a few
    // centimeters apart: the union leaves slivers beside them, and its
    // right wall lies off the union where the arm leaves it.
    {QStringLiteral("a10"), cwPassageTubeMeshCheck::SliverTriangle},
    {QStringLiteral("a10"), cwPassageTubeMeshCheck::WallMissed},
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
// that cross, shows only in the build: see overlapsMouths.
bool joinsAtGrazingAngle(const Cases::Case& entry)
{
    return planJoinDegrees(entry.trip) <= kGrazingJoinDegrees + kAngleToleranceDegrees;
}

// A through turning 90° at J turns the corner at J's rings, which a single
// level arm no larger than the through opens into cleanly. A sloped arm,
// two arms or an arm larger than the through meet the corner's tight inner
// wall or straddle the turn, and the zip band folds or crosses it
// (Crease, SelfIntersection, SliverTriangle).
bool bendsSharplyAtJunction(const Cases::Case& entry)
{
    const QStringList& tags = entry.tags;
    const bool isSmallLevelArm = tags.contains(QStringLiteral("profile:0"))
                                 && tags.contains(QStringLiteral("layout:single"))
                                 && !tags.contains(QStringLiteral("ratio:2.5"));
    return tags.contains(QStringLiteral("bend:90")) && !isSmallLevelArm;
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
// - three small arms, one or more meeting the through obliquely or sloping,
//   where an arm's strip folds against its zip band at the mouth by 150° to
//   160° (Crease, SelfIntersection);
// - a sloped arm meets a through bent 40° at J, and the zip band bridges a
//   bin jump with thin or folded triangles (Crease, SliverTriangle,
//   SelfIntersection);
// - two or three arms share one window at a through bent 40° at J, sloped
//   or as big as the through, and the zip band folds against an arm's
//   strip where their mouths come within a few centimeters (Crease,
//   SliverTriangle, SelfIntersection);
// - two arms that share one window at a straight through, oblique or
//   steep, where the zip band folds against a strip at the neck between
//   their mouths (Crease, SelfIntersection);
// - a level arm 2.5 times the through's size, which takes the through, so
//   the through holds both J's sections, stitched until T07ah3 caps them
//   with their hull (Crease, SelfIntersection, FoldBack, DegenerateTriangle,
//   FrameTwist);
// - small arms sloping down 30 degrees at plan 30 beside a through bent 40
//   at J, where an arm's strip folds against its zip band by about 150°
//   (Crease);
// - a small level arm at plan 30 beside a through bent 90 at J, where the
//   zip band takes a sliver (SliverTriangle).
const QStringList kOpenSweepCases {
    QStringLiteral("plan 90, profile 0, ratio 0.3, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 90, profile 30, ratio 0.3, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 90, profile 30, ratio 0.3, 3 arms, bend 40, section x2 past J"),
    QStringLiteral("plan 90, profile -30, ratio 0.3, 3 arms, bend 40, section x2 past J"),
    QStringLiteral("plan 60, profile -30, ratio 0.3, 3 arms, bend 40, section x2 past J"),
    QStringLiteral("plan 45, profile 0, ratio 0.3, 3 arms, bend 0, same section"),
    QStringLiteral("plan 45, profile 30, ratio 0.3, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 30, profile 30, ratio 0.3, 3 arms, bend 0, section x2 past J"),

    QStringLiteral("plan 30, profile 30, ratio 1, 1 arm, bend 40, same section"),
    QStringLiteral("plan 30, profile 30, ratio 1, 1 arm, bend 40, section x2 past J"),
    QStringLiteral("plan 30, profile -30, ratio 1, 1 arm, bend 40, same section"),
    QStringLiteral("plan 90, profile -30, ratio 1, 1 arm, bend 40, same section"),

    QStringLiteral("plan 30, profile -60, ratio 0.3, 3 arms, bend 40, same section"),
    QStringLiteral("plan 30, profile 0, ratio 1, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 30, profile 30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 30, profile 30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 30, profile 60, ratio 0.3, 3 arms, bend 40, same section"),
    QStringLiteral("plan 45, profile -30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 45, profile -30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 45, profile -60, ratio 0.3, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 45, profile -60, ratio 0.3, 3 arms, bend 40, same section"),
    QStringLiteral("plan 45, profile 30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 45, profile 30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 60, profile -30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 60, profile -30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 60, profile -60, ratio 0.3, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 60, profile -60, ratio 0.3, 3 arms, bend 40, same section"),
    QStringLiteral("plan 60, profile 0, ratio 1, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 60, profile 0, ratio 1, 2 arms opposite, bend 40, section x2 past J"),
    QStringLiteral("plan 60, profile 30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 60, profile 30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 90, profile -30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 90, profile -30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 90, profile -30, ratio 1, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 90, profile -60, ratio 0.3, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 90, profile 30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 90, profile 30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),

    QStringLiteral("plan 30, profile -60, ratio 0.3, 2 arms opposite, bend 0, section x2 past J"),
    QStringLiteral("plan 30, profile 0, ratio 1, 2 arms opposite, bend 0, same section"),
    QStringLiteral("plan 30, profile 60, ratio 0.3, 2 arms same side, bend 0, section x2 past J"),
    QStringLiteral("plan 45, profile 60, ratio 0.3, 2 arms same side, bend 0, section x2 past J"),

    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 0, same section"),
    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"),
    QStringLiteral("plan 30, profile 0, ratio 2.5, 3 arms, bend 0, same section"),
    QStringLiteral("plan 30, profile 0, ratio 2.5, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 0, same section"),
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"),
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms opposite, bend 0, same section"),
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"),
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 0, same section"),
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"),
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms opposite, bend 0, same section"),
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"),
    QStringLiteral("plan 90, profile 0, ratio 2.5, 1 arm, bend 0, same section"),
    QStringLiteral("plan 90, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"),
    QStringLiteral("plan 90, profile 0, ratio 2.5, 3 arms, bend 0, same section"),
    QStringLiteral("plan 90, profile 0, ratio 2.5, 3 arms, bend 0, section x2 past J"),

    QStringLiteral("plan 30, profile -30, ratio 0.3, 1 arm, bend 40, same section"),
    QStringLiteral("plan 30, profile -30, ratio 0.3, 1 arm, bend 40, section x2 past J"),
    QStringLiteral("plan 30, profile -30, ratio 0.3, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 30, profile -30, ratio 0.3, 2 arms opposite, bend 40, section x2 past J"),
    QStringLiteral("plan 30, profile -30, ratio 0.3, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 30, profile -30, ratio 0.3, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 30, profile -30, ratio 0.3, 3 arms, bend 40, same section"),
    QStringLiteral("plan 30, profile -30, ratio 0.3, 3 arms, bend 40, section x2 past J"),

    QStringLiteral("plan 30, profile 0, ratio 0.3, 1 arm, bend 90, same section"),
    QStringLiteral("plan 30, profile 0, ratio 0.3, 1 arm, bend 90, section x2 past J"),
};

// An arm sloping 60 degrees is steep, so its sections are round at the mean
// of their left and right until T07ah2 shapes them, and a side wall reads
// past the circle (WallMissed).
bool hasSteepArm(const Cases::Case& entry)
{
    return entry.tags.contains(QStringLiteral("profile:60")) || entry.tags.contains(QStringLiteral("profile:-60"));
}

// Cases that build clean with the stitch and not as the union (T07ak):
// - 8's pit leaves the through's floor reading at J off the union (WallMissed);
// - an arm whose floor lies in the bent through's floor plane folds where
//   the two floors meet (Crease, SliverTriangle);
// - two big arms on one side slide their caps back along their legs, which
//   folds the rings behind them (FoldBack) and leaves a wall off the tube
//   (WallMissed);
// - a sloping arm's slid cap folds against its wall at a through with two
//   sections at J (Crease, WallMissed).
// The sweep's clean subsample skips them where it samples them.
const QStringList kUnionOpenCases {
    QStringLiteral("8. Pit dropping into a passage"),
    QStringLiteral("plan 90, profile 0, ratio 1, 1 arm, bend 90, same section"),
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms same side, bend 0, same section"),
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms same side, bend 40, section x2 past J"),
};

// Sweep cases kept out of the clean checks by their parameters or name.
bool isOpenSweepCase(const Cases::Case& entry)
{
    return joinsAtGrazingAngle(entry)
           || bendsSharplyAtJunction(entry)
           || hasLargeTiltedArm(entry)
           || hasSteepArm(entry)
           || kOpenSweepCases.contains(entry.name);
}

// Two arms whose mouths cross on the through's surface, the merge's other
// stated limit: the passages run into each other above the through and the
// second falls back to a fan cap inside the first (SelfIntersection). Arms
// whose mouths only share through quads are zipped in one window.
bool overlapsMouths(const cwPassageTubeBuilder::Debug& debug)
{
    return std::any_of(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == cwPassageTubeMesh::Warning::JunctionOverlap
               && warning.message.contains(QStringLiteral("run into each other"));
    });
}

// Every check, apart from the cases whose mouths overlap, which the
// [!mayfail] sweep tests cover, and the union's open cases.
void checkSweepCaseBuildsClean(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    if (kUnionOpenCases.contains(entry.name)) {
        return;
    }
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

int warningCount(const cwPassageTubeBuilder::Debug& debug, cwPassageTubeMesh::Warning::Type type)
{
    return static_cast<int>(std::count_if(debug.warnings.cbegin(), debug.warnings.cend(), [type](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == type;
    }));
}

cwPassageTubeBuilder::Debug buildDebug(const Cases::Survey& survey, double sampleSpacing)
{
    return cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
}

QSet<QString> chainStations(const cwPassageTubeBuilder::Debug& debug, const cwPassageTubeBuilder::Chain& chain)
{
    QSet<QString> names;
    for (const cwPassageTubeBuilder::ChainLeg& chainLeg : chain.legs) {
        names.insert(debug.legs.at(chainLeg.legIndex).from);
        names.insert(debug.legs.at(chainLeg.legIndex).to);
    }
    return names;
}

// The chains whose mouths a window of chain zips.
QSet<int> zippedArms(const cwPassageTubeBuilder::Chain& chain)
{
    QSet<int> arms;
    for (const cwPassageTubeBuilder::JunctionWindow& window : chain.windows) {
        for (const cwPassageTubeBuilder::WindowMouth& mouth : window.mouths) {
            arms.insert(mouth.armChain);
        }
    }
    return arms;
}

// The single-leg chain from station to J.
int armChain(const cwPassageTubeBuilder::Debug& debug, const QString& station)
{
    for (int c = 0; c < debug.chains.size(); ++c) {
        const QSet<QString> stations = chainStations(debug, debug.chains.at(c));
        if (debug.chains.at(c).legs.size() == 1 && stations.contains(station)) {
            return c;
        }
    }
    return -1;
}

// Sweep junctions whose arms the stitch still leaves unzipped, by family:
// - two arms 2.5 times the through's size 20 degrees off it at a through
//   bent 40 degrees, and three arms as big as the through at a bend of 40,
//   where an arm's mouth wraps around the through;
// - arms 2.5 times the through's size sloping down 30 degrees at a bend of
//   90 or with three arms, and a small arm climbing 30 degrees beside an
//   opposite one at a bend of 90, where a column's mouth point leaves the
//   through's wall;
// - small arms at a through bent 90 degrees, three at plan 20 or two
//   opposite at plan 90, and two arms 2.5 times the through's size sloping
//   30 degrees opposite each other at a bend of 90, whose mouths run
//   backward around the through;
// - three sloping arms 2.5 times the through's size, and two arms as big as
//   the through on one side at a bend of 90, where the through holds two of
//   J's sections, stitched until T07ah3 caps them with their hull.
const QStringList kUnzippedSweepCases {
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms opposite, bend 40, same section"),
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"),
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms same side, bend 40, same section"),
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms same side, bend 40, section x2 past J"),
    QStringLiteral("plan 90, profile 0, ratio 1, 3 arms, bend 40, section x2 past J"),

    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 90, same section"),
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"),
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 90, same section"),
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"),
    QStringLiteral("plan 45, profile -30, ratio 2.5, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 45, profile -30, ratio 2.5, 3 arms, bend 90, section x2 past J"),
    QStringLiteral("plan 90, profile -30, ratio 2.5, 3 arms, bend 40, same section"),
    QStringLiteral("plan 90, profile 30, ratio 0.3, 2 arms opposite, bend 90, section x2 past J"),

    QStringLiteral("plan 20, profile 0, ratio 0.3, 3 arms, bend 90, same section"),
    QStringLiteral("plan 20, profile 0, ratio 0.3, 3 arms, bend 90, section x2 past J"),
    QStringLiteral("plan 90, profile 0, ratio 0.3, 2 arms opposite, bend 90, same section"),
    QStringLiteral("plan 90, profile 0, ratio 0.3, 2 arms opposite, bend 90, section x2 past J"),
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"),
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"),

    QStringLiteral("plan 20, profile -30, ratio 2.5, 3 arms, bend 90, same section"),
    QStringLiteral("plan 20, profile -30, ratio 2.5, 3 arms, bend 90, section x2 past J"),
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 0, same section"),
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 90, same section"),
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 90, section x2 past J"),
    QStringLiteral("plan 30, profile -30, ratio 2.5, 3 arms, bend 90, section x2 past J"),
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 90, same section"),
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 90, section x2 past J"),
    QStringLiteral("plan 60, profile -30, ratio 2.5, 3 arms, bend 0, section x2 past J"),
    QStringLiteral("plan 60, profile 0, ratio 1, 2 arms same side, bend 90, section x2 past J"),
    QStringLiteral("plan 90, profile -30, ratio 2.5, 3 arms, bend 40, section x2 past J"),
};

void checkJoinsEveryArm(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    const cwPassageTubeBuilder::Debug debug = buildDebug(Cases::toSurvey(entry), entry.sampleSpacing);
    CHECK(warningCount(debug, cwPassageTubeMesh::Warning::JunctionUnzipped) == 0);
}

bool isJoinedJunctionCase(const Cases::Case& entry)
{
    const bool isJunctionCase = entry.tags.contains(QStringLiteral("gallery")) || isSweep(entry);
    return isJunctionCase && hasJunction(entry) && !kUnzippedSweepCases.contains(entry.name);
}

std::pair<unsigned int, int> randomSurvey(int seed)
{
    return {static_cast<unsigned int>(seed), 1 + (seed - 1) % kMaxRandomJunctions};
}

// Random seeds whose union is unclean, at the ring rules T07al and T07am
// change: a cap slid back along its leg folds the rings behind it
// (FoldBack) and leaves walls off the tube (WallMissed), two sections at a
// station or a slid cap fold against a wall (Crease), and walls that touch
// along a line leave slivers (SliverTriangle).
const QVector<int> kJunctionLimitRandomSeeds {1, 6, 7, 9, 11, 15, 16, 17, 18, 20, 24, 28, 29, 30, 32, 33, 34, 35, 40, 41,
                                             42, 44, 46, 47, 48};

bool isJunctionLimitRandomSeed(int seed)
{
    return kJunctionLimitRandomSeeds.contains(seed);
}

// 25's shaft foot joins a plumb section to a level one, whose union leaves
// slivers (SliverTriangle) until T07am stands its rings vertical.
const QStringList kOpenJunctionFreeCases {
    QStringLiteral("25. Zero floor reading at the foot of a shaft"),
};

// Catalog junctions whose union is unclean: 19's through doubles back
// beside a big arm, and the arm's slid cap folds against its wall (Crease).
const QStringList kOpenCatalogJunctionCases {
    QStringLiteral("19. Through doubling back beside a big arm"),
};

// Random seeds that build clean with the stitch and not as the union: 14's
// slid cap folds against its wall (Crease).
const QVector<int> kUnionOpenRandomSeeds {14};

// Random seeds whose passages hold a plumb and a level section at a shaft's
// lip or foot, whose union leaves slivers and folds (Crease,
// SliverTriangle, DegenerateTriangle) until T07am stands their rings
// vertical.
const QVector<int> kOpenJunctionFreeRandomSeeds {32};

// Wild random surveys, as (seed, junctions), whose union leaves a zero-area
// triangle (DegenerateTriangle): 40 with 4 junctions holds a plumb and a
// level section at a station, and 7 with 1 junction, clean with the stitch,
// folds a slid cap against its wall.
const QVector<std::pair<int, int>> kDegenerateWildSurveys {{7, 1}, {40, 4}};

void checkCenterlineInside(const std::string& name)
{
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    const QVector<cwPassageTubeMeshCheck::Issue> issues = cwPassageTubeMeshCheck::centerlineInside(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText({issues, 0, {}}).toStdString());
    CHECK(issues.isEmpty());
}

} // namespace

TEST_CASE("cwPassageTubeCases catalog builds clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return !isSweep(entry) && hasJunction(entry) && entry.name != kPhakeCave && !kOpenCatalogJunctionCases.contains(entry.name)
               && !kUnionOpenCases.contains(entry.name);
    })));
    checkCaseBuildsClean(name);
}

TEST_CASE("cwPassageTubeCases open catalog junctions build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return kOpenCatalogJunctionCases.contains(entry.name);
    })));
    checkCaseBuildsClean(name);
}

TEST_CASE("cwPassageTubeCases union regressions build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    SECTION("catalog and sweep cases")
    {
        const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
            return kUnionOpenCases.contains(entry.name);
        })));
        checkCaseBuildsClean(name);
    }
    SECTION("random surveys")
    {
        const auto [seed, junctions] = randomSurvey(GENERATE(from_range(kUnionOpenRandomSeeds)));
        INFO("seed " << seed << ", " << junctions << " junctions");
        checkBuildsClean(Cases::random(seed, junctions));
    }
}

// The zip leaves Crease, SelfIntersection and FoldBack on these big arms;
// the union of the chains' capped tubes builds them clean (T07ak).
TEST_CASE("cwPassageTubeCases big arms build clean through the union", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(as<std::string>(),
                                      "3. Big arm into a small passage",
                                      "16. Arm with its own section at J larger than the through",
                                      "18. Three big arms into a small passage");
    checkCaseBuildsClean(name);
}

// An arm cut back to the first of its columns that touches a big through
// leaves its bends, and the stations at them, outside the tube.
TEST_CASE("cwPassageTubeCases keeps the centerline inside a junction arm", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(std::string("21. Small arm along the wall of a big room"),
                                      std::string("22. Default-radius arm bending inside a big through's reach"));
    checkCenterlineInside(name);
}

// A corner station's ring holds the station although it reads 0 on the
// outside wall: 23 reads 0 there, and 24 borrows the 0 of its first station.
TEST_CASE("cwPassageTubeCases keeps a corner station inside its ring", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(std::string("23. Sharp bend at a station reading zero on the outside wall"),
                                      std::string("24. Sharp bend in an unread passage next to a zero reading"));
    checkCenterlineInside(name);
}

// A bend's rings keep within the station's readings: these junctions bend
// their through at J, 23 bends at c1 and ends on a plumb ring at c0, and
// Phake Cave bends sharply from a2 to a9.
TEST_CASE("cwPassageTubeCases keeps a bend's rings within the station's readings", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(std::string("3. Big arm into a small passage"),
                                      std::string("10. Passage turning straight down a pit, side arm at the lip"),
                                      std::string("16. Arm with its own section at J larger than the through"),
                                      std::string("18. Three big arms into a small passage"),
                                      std::string("19. Through doubling back beside a big arm"),
                                      std::string("23. Sharp bend at a station reading zero on the outside wall"),
                                      kPhakeCave.toStdString());
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    const cwPassageTubeBuilder::Debug debug = buildDebug(Cases::toSurvey(entry), entry.sampleSpacing);
    const QVector<cwPassageTubeMeshCheck::Issue> issues = cwPassageTubeMeshCheck::ringIssues(debug);
    INFO(cwPassageTubeMeshCheck::toText({issues, 0, {}}).toStdString());
    CHECK(std::none_of(issues.cbegin(), issues.cend(), [](const cwPassageTubeMeshCheck::Issue& issue) {
        return issue.type == cwPassageTubeMeshCheck::WallBeyondReading;
    }));
}

// The arm's ceiling runs level with the through's, so its mouth runs along
// the through's ceiling for several meters. The zip band to that long edge
// takes no slivers; the one crease left is the 20° wedge at the mouth's far
// tip, where the arm's wall folds against the through's (see
// joinsAtGrazingAngle).
// The builder unites the chains' tubes instead of stitching junctions
// (T07ak); T07an deletes the stitch and this test.
TEST_CASE("cwPassageTubeCases zips a mouth that runs along the through's ceiling", "[cwPassageTubeCases][JunctionStitch][.]")
{
    constexpr float kOnMouthMeters = 1e-3f;
    const Cases::Case& entry = catalogCase("plan 20, profile 0, ratio 1, 1 arm, bend 0, same section");
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    REQUIRE(debug.chains.size() == 2);
    REQUIRE(debug.chains.at(0).windows.size() == 1);
    REQUIRE(debug.chains.at(0).windows.first().mouths.size() == 1);
    const cwPassageTubeBuilder::WindowMouth& windowMouth = debug.chains.at(0).windows.first().mouths.first();
    const cwPassageTubeBuilder::Chain& arm = debug.chains.at(windowMouth.armChain);
    const QVector<QVector3D> mouth = mouthPositions(windowMouth.isArmStart ? arm.startMouth : arm.endMouth);
    REQUIRE(mouth.size() >= cwPassageTubeBuilder::Options().ringResolution);

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
// The builder unites the chains' tubes instead of stitching junctions
// (T07ak); T07an deletes the stitch and this test.
TEST_CASE("cwPassageTubeCases re-grows the through after dropping an arm", "[cwPassageTubeCases][JunctionStitch][.]")
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

// The arm is 2.5 times the through passage's size at J, so the through runs
// along it and on into one half of the small passage; the other half ends
// at J, inside it.
TEST_CASE("cwPassageTubeCases chains the big passage as the through", "[PassageTube][cwPassageTubeCases]")
{
    const Cases::Case& entry = catalogCase("3. Big arm into a small passage");
    const Cases::Survey survey = Cases::toSurvey(entry.trip);
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(entry.sampleSpacing));
    REQUIRE(debug.chains.size() == 2);

    const auto stations = [&](const cwPassageTubeBuilder::Chain& chain) {
        QSet<QString> names;
        for (const cwPassageTubeBuilder::ChainLeg& chainLeg : chain.legs) {
            names.insert(debug.legs.at(chainLeg.legIndex).from);
            names.insert(debug.legs.at(chainLeg.legIndex).to);
        }
        return names;
    };
    const auto throughIt = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [](const cwPassageTubeBuilder::Chain& chain) {
        return chain.legs.size() == 2;
    });
    REQUIRE(throughIt != debug.chains.cend());
    const cwPassageTubeBuilder::Chain& through = *throughIt;
    const cwPassageTubeBuilder::Chain& small = debug.chains.at(throughIt == debug.chains.cbegin() ? 1 : 0);
    const QSet<QString> throughStations = stations(through);
    CHECK(throughStations.contains(QStringLiteral("a00")));
    CHECK(throughStations.contains(QStringLiteral("J")));

    REQUIRE(small.legs.size() == 1);
    const bool isToJunction = debug.legs.at(small.legs.first().legIndex).to == QStringLiteral("J");
    const bool endsAtJunction = isToJunction != small.legs.first().reversed;
    CHECK((endsAtJunction ? small.endCap : small.startCap) == cwPassageTubeBuilder::EndCap::Mouth);
    checkCaseBuildsClosed(entry.name.toStdString());
}

// No two of the three big arms continue each other as straight as the small
// passage does, and every big arm ties the others, so only the size rule
// makes two of them the through: the pair 90 degrees apart, since the
// straighter pairs would leave the small passage beside the third big arm.
TEST_CASE("cwPassageTubeCases chains two big arms as the through", "[PassageTube][cwPassageTubeCases]")
{
    const Cases::Case& entry = catalogCase("18. Three big arms into a small passage");
    const cwPassageTubeBuilder::Debug debug = buildDebug(Cases::toSurvey(entry), entry.sampleSpacing);
    const auto through = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [](const cwPassageTubeBuilder::Chain& chain) {
        return chain.legs.size() == 2;
    });
    REQUIRE(through != debug.chains.cend());
    const QSet<QString> stations = chainStations(debug, *through);
    CHECK(stations == QSet<QString>({QStringLiteral("a00"), QStringLiteral("J"), QStringLiteral("a10")}));
    checkCaseBuildsClosed(entry.name.toStdString());
}

// The big arm leaves J between the two halves of the through passage, which
// bends 90 degrees there, so no leg continues it within the chain turn
// limit. It still claims the through, ends past J, and both halves of the
// small passage open into its wall.
TEST_CASE("cwPassageTubeCases ends a big dead-end through past the junction", "[PassageTube][cwPassageTubeCases]")
{
    constexpr double kBetweenHalvesDegrees = 315.0;
    constexpr double kBigArmRatio = 2.5;
    const Cases::Section section {1.2, 1.2, 1.5, 1.0};
    Cases::JunctionCase junctionCase;
    junctionCase.name = QStringLiteral("Big arm between the halves of a bent through");
    junctionCase.throughSection = section;
    junctionCase.throughAtJunction = section;
    junctionCase.throughPastJunction = section;
    junctionCase.throughBendDegrees = kRightAngleDegrees;
    Cases::ArmSpec arm;
    arm.planDegrees = kBetweenHalvesDegrees;
    arm.at = section.scaled(kBigArmRatio);
    arm.atJunction = arm.at;
    junctionCase.arms = {arm};
    const Cases::Survey survey = Cases::toSurvey(junctionCase);
    const cwPassageTubeBuilder::Debug debug = buildDebug(survey, junctionCase.sampleSpacing);
    const int bigChain = armChain(debug, QStringLiteral("a00"));
    REQUIRE(bigChain >= 0);
    const cwPassageTubeBuilder::Chain& big = debug.chains.at(bigChain);
    const cwPassageTubeBuilder::Leg& bigLeg = debug.legs.at(big.legs.first().legIndex);
    const bool endsAtJunction = (bigLeg.to == QStringLiteral("J")) != big.legs.first().reversed;
    const double reach = endsAtJunction ? big.endThroughReachMeters : big.startThroughReachMeters;
    CHECK(reach > section.up);

    // The end ring at J lies past J along the big passage.
    const Cases::Trip trip = Cases::toTrip(junctionCase);
    const QVector3D junction = position(trip, QStringLiteral("J"));
    const QVector3D arriving = (junction - position(trip, QStringLiteral("a00"))).normalized();
    const cwPassageTubeBuilder::Ring& endRing = endsAtJunction ? big.rings.last() : big.rings.first();
    CHECK(QVector3D::dotProduct(endRing.center - junction, arriving) > 0.0f);

    checkBuildsClosed(survey, junctionCase.sampleSpacing);
    const cwPassageTubeMeshCheck::Report report = cwPassageTubeMeshCheck::check(debug.mesh, debug);
    INFO(cwPassageTubeMeshCheck::toText(report).toStdString());
    CHECK(report.issues.isEmpty());
}

// The big arm continues the through's far half, which turns back toward it,
// so the near half leaves J 30 degrees from the big arm, grazing its wall,
// and still opens into it.
TEST_CASE("cwPassageTubeCases opens the leg beside a big arm", "[PassageTube][cwPassageTubeCases]")
{
    const Cases::Case& entry = catalogCase("19. Through doubling back beside a big arm");
    const cwPassageTubeBuilder::Debug debug = buildDebug(Cases::toSurvey(entry), entry.sampleSpacing);
    REQUIRE(armChain(debug, QStringLiteral("t0")) >= 0);
    const auto through = std::find_if(debug.chains.cbegin(), debug.chains.cend(), [](const cwPassageTubeBuilder::Chain& chain) {
        return chain.legs.size() == 2;
    });
    REQUIRE(through != debug.chains.cend());
    CHECK(chainStations(debug, *through).contains(QStringLiteral("a00")));
    checkCaseBuildsClosed(entry.name.toStdString());
}

// Every gallery junction and a subsample of the sweep's join their arms into
// the through, apart from the families in kUnzippedSweepCases.
// The builder unites the chains' tubes instead of stitching junctions
// (T07ak); T07an deletes the stitch and this test.
TEST_CASE("cwPassageTubeCases joins every arm", "[cwPassageTubeCases][JunctionStitch][.]")
{
    std::vector<std::string> names = caseNames([](const Cases::Case& entry) {
        return isJoinedJunctionCase(entry) && !isSweep(entry);
    });
    const std::vector<std::string> sweep = caseNames([](const Cases::Case& entry) {
        return isJoinedJunctionCase(entry) && isSweep(entry);
    }, kSmokeStride);
    names.insert(names.end(), sweep.cbegin(), sweep.cend());
    const std::string name = GENERATE_COPY(from_range(names));
    checkJoinsEveryArm(name);
}

// The builder unites the chains' tubes instead of stitching junctions
// (T07ak); T07an deletes the stitch and this test.
TEST_CASE("cwPassageTubeCases full sweep joins every arm", "[cwPassageTubeCases][Sweep][JunctionStitch][.]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isJoinedJunctionCase(entry) && isSweep(entry);
    })));
    checkJoinsEveryArm(name);
}

// Two arms as big as the through leave it on opposite sides, straight on
// and where it bends 40 degrees past J.
TEST_CASE("cwPassageTubeCases unites opposite arms as big as the through", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(as<std::string>(),
                                      "plan 90, profile 0, ratio 1, 2 arms opposite, bend 0, same section",
                                      "plan 90, profile 0, ratio 1, 2 arms opposite, bend 40, same section");
    checkCaseBuildsClosed(name);
    checkCaseBuildsClean(name);
}

// Arms as big as the through whose mouths meet around it.
TEST_CASE("cwPassageTubeCases closes the union where big arms' mouths meet", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(as<std::string>(),
                                      "plan 20, profile 0, ratio 1, 2 arms opposite, bend 0, same section",
                                      "plan 20, profile 0, ratio 1, 3 arms, bend 90, same section",
                                      "plan 30, profile 0, ratio 1, 2 arms opposite, bend 40, same section",
                                      "plan 45, profile 0, ratio 1, 2 arms opposite, bend 40, same section",
                                      "plan 60, profile 30, ratio 1, 3 arms, bend 40, same section",
                                      "plan 90, profile 0, ratio 1, 2 arms same side, bend 90, same section",
                                      "plan 20, profile -30, ratio 1, 3 arms, bend 90, section x2 past J");
    checkCaseBuildsClosed(name);
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
// overlapsMouths for what they report. Passages that run into each other
// are warnings; what fails is a zip band that crosses or folds against the
// arm it joins or a second arm (SelfIntersection, Crease, SliverTriangle).
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
        return !hasJunction(entry) && !kOpenJunctionFreeCases.contains(entry.name);
    })));
    checkCaseBuildsClean(name);
}

TEST_CASE("cwPassageTubeCases open catalog cases without a junction build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return kOpenJunctionFreeCases.contains(entry.name);
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
    const Cases::Survey survey = Cases::toSurvey(entry);
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

TEST_CASE("cwPassageTubeCases random surveys build clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const auto [seed, junctions] = randomSurvey(GENERATE(filter([](int candidate) {
        return !isJunctionLimitRandomSeed(candidate) && !kUnionOpenRandomSeeds.contains(candidate);
    }, range(1, kRandomSeedCount + 1))));
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClean(Cases::random(seed, junctions));
}

TEST_CASE("cwPassageTubeCases random surveys with junctions at the merge's limits build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const auto [seed, junctions] = randomSurvey(GENERATE(from_range(kJunctionLimitRandomSeeds)));
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClean(Cases::random(seed, junctions));
}

// Seed 9's passage from s8 folds across itself; the union takes its tube as
// it is, and the builder names it.
TEST_CASE("cwPassageTubeCases warns of a chain tube that crosses itself", "[PassageTube][cwPassageTubeCases]")
{
    constexpr int kSelfCrossingSeed = 9;
    const auto [seed, junctions] = randomSurvey(kSelfCrossingSeed);
    const cwPassageTubeBuilder::Debug debug = buildDebug(Cases::toSurvey(Cases::random(seed, junctions)), cwPassageTubeBuilder::Options().sampleSpacingMeters);
    const auto crossing = std::find_if(debug.warnings.cbegin(), debug.warnings.cend(), [](const cwPassageTubeMesh::Warning& warning) {
        return warning.type == cwPassageTubeMesh::Warning::ChainSelfCrossing;
    });
    REQUIRE(crossing != debug.warnings.cend());
    INFO(crossing->message.toStdString());
    CHECK(crossing->station == QStringLiteral("s8"));
    CHECK(crossing->message.contains(QStringLiteral("station s8 crosses itself at ")));

    const QVector<cwPassageTubeUnion::ChainTube> tubes = cwPassageTubeUnion::chainTubes(debug);
    CHECK(std::any_of(tubes.cbegin(), tubes.cend(), [&](const cwPassageTubeUnion::ChainTube& tube) {
        const cwPassageTubeBuilder::ChainLeg& first = debug.chains.at(tube.chain).legs.first();
        const cwPassageTubeBuilder::Leg& leg = debug.legs.at(first.legIndex);
        const bool startsAtCrossing = (first.reversed ? leg.to : leg.from) == crossing->station;
        return startsAtCrossing && !cwPassageTubeMeshCheck::selfIntersections(tube.mesh, cwPassageTubeMeshCheck::wholeMesh(tube.mesh)).isEmpty();
    }));
    checkBuildsClosed(Cases::random(seed, junctions));
}

TEST_CASE("cwPassageTubeCases random surveys build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const auto [seed, junctions] = randomSurvey(GENERATE(range(1, kRandomSeedCount + 1)));
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClosed(Cases::random(seed, junctions));
}

TEST_CASE("cwPassageTubeCases random surveys without a junction build clean meshes", "[PassageTube][cwPassageTubeCases]")
{
    const int seed = GENERATE(filter([](int candidate) {
        return !kOpenJunctionFreeRandomSeeds.contains(candidate);
    }, range(1, kRandomSeedCount + 1)));
    INFO("seed " << seed);
    checkBuildsClean(Cases::random(static_cast<unsigned int>(seed), 0));
}

TEST_CASE("cwPassageTubeCases open random surveys without a junction build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const int seed = GENERATE(from_range(kOpenJunctionFreeRandomSeeds));
    INFO("seed " << seed);
    checkBuildsClean(Cases::random(static_cast<unsigned int>(seed), 0));
}

TEST_CASE("cwPassageTubeCases random surveys without a junction build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const int seed = GENERATE(range(1, kRandomSeedCount + 1));
    INFO("seed " << seed);
    checkBuildsClosed(Cases::random(static_cast<unsigned int>(seed), 0));
}

TEST_CASE("cwPassageTubeCases random surveys are cave-like", "[PassageTube][cwPassageTubeCases]")
{
    const auto [seed, junctions] = randomSurvey(GENERATE(range(1, kRandomSeedCount + 1)));
    INFO("seed " << seed << ", " << junctions << " junctions");
    const Cases::Trip trip = Cases::random(seed, junctions);

    for (int c = 0; c < trip.chunks.size(); ++c) {
        const QList<Cases::Station>& chunk = trip.chunks.at(c);
        for (int i = 2; i < chunk.size(); ++i) {
            const QVector3D before = *chunk.at(i - 1).position - *chunk.at(i - 2).position;
            const QVector3D after = *chunk.at(i).position - *chunk.at(i - 1).position;
            const double turn = std::abs(std::remainder(qRadiansToDegrees(std::atan2(after.x(), after.y()) - std::atan2(before.x(), before.y())), 360.0));
            INFO("chunk " << c << ", station " << chunk.at(i - 1).name.toStdString());
            CHECK(turn <= Cases::kRandomMaxTurnDegrees + kAngleToleranceDegrees);
        }
        for (int i = 1; i < chunk.size(); ++i) {
            for (int reading : {kLeftReading, kRightReading}) {
                const double ratio = chunk.at(i).lrud.at(reading).toDouble() / chunk.at(i - 1).lrud.at(reading).toDouble();
                CHECK(ratio <= Cases::kRandomMaxLrudStep + kLrudStepTolerance);
                CHECK(ratio >= 1.0 / Cases::kRandomMaxLrudStep - kLrudStepTolerance);
            }
        }

        for (int i = 1; i < chunk.size(); ++i) {
            const QVector3D leg = *chunk.at(i).position - *chunk.at(i - 1).position;
            const double inclination = qRadiansToDegrees(std::atan2(std::abs(leg.z()), std::hypot(leg.x(), leg.y())));
            if (inclination < Cases::kRandomShaftDegrees + kAngleToleranceDegrees) {
                continue;
            }
            const bool isDescending = leg.z() < 0.0f;
            const Cases::Station& upper = isDescending ? chunk.at(i - 1) : chunk.at(i);
            const Cases::Station& lower = isDescending ? chunk.at(i) : chunk.at(i - 1);
            const double depth = std::abs(leg.z());
            INFO("chunk " << c << ", shaft " << upper.name.toStdString() << " to " << lower.name.toStdString() << ", " << depth << " m deep");
            CHECK(upper.lrud.at(kDownReading).toDouble() >= depth * (1.0 - kLrudStepTolerance));
            CHECK(lower.lrud.at(kUpReading).toDouble() >= depth * (1.0 - kLrudStepTolerance));
        }
    }

    for (int c = 1; c < trip.chunks.size(); ++c) {
        const QList<Cases::Station>& branch = trip.chunks.at(c);
        const Cases::Station& junction = branch.first();
        const QVector3D firstLeg = *branch.at(1).position - *junction.position;
        for (int earlier = 0; earlier < c; ++earlier) {
            const QList<Cases::Station>& passage = trip.chunks.at(earlier);
            for (int i = 0; i < passage.size(); ++i) {
                if (passage.at(i).name != junction.name) {
                    continue;
                }
                for (int neighbor : {i - 1, i + 1}) {
                    if (neighbor < 0 || neighbor >= passage.size()) {
                        continue;
                    }
                    INFO("branch " << c << " at " << junction.name.toStdString() << " against " << passage.at(neighbor).name.toStdString());
                    CHECK(angleBetweenDegrees(firstLeg, *passage.at(neighbor).position - *junction.position)
                          >= Cases::kRandomMinBranchDegrees - kAngleToleranceDegrees);
                }
            }
        }
    }

    for (int c = 0; c < trip.chunks.size(); ++c) {
        for (const Cases::Station& station : trip.chunks.at(c)) {
            for (int other = 0; other < trip.chunks.size(); ++other) {
                const QList<Cases::Station>& legs = trip.chunks.at(other);
                for (int i = 1; i < legs.size(); ++i) {
                    if (other == c || legs.at(i - 1).name == station.name || legs.at(i).name == station.name) {
                        continue;
                    }
                    INFO("station " << station.name.toStdString() << " against " << legs.at(i - 1).name.toStdString()
                                    << " to " << legs.at(i).name.toStdString());
                    CHECK(distanceToLeg(*station.position, *legs.at(i - 1).position, *legs.at(i).position)
                          >= Cases::kRandomClearanceMeters - kPulledToleranceMeters);
                }
            }
        }
    }
}

TEST_CASE("cwPassageTubeCases wild random surveys build closed tubes", "[PassageTube][cwPassageTubeCases]")
{
    const auto [seed, junctions] = randomSurvey(GENERATE(range(1, kRandomSeedCount + 1)));
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClosed(Cases::randomWild(seed, junctions));
    checkBuildsClosed(Cases::randomWild(seed, 0));
}

// Sharp random bends no longer fold the inner wall into zero-area triangles.
TEST_CASE("cwPassageTubeCases wild random surveys build no degenerate triangles", "[PassageTube][cwPassageTubeCases]")
{
    const auto [seed, junctions] = randomSurvey(GENERATE(range(1, kRandomSeedCount + 1)));
    const int junctionCount = GENERATE_COPY(0, junctions);
    INFO("seed " << seed << ", " << junctionCount << " junctions");
    if (kDegenerateWildSurveys.contains(std::pair {static_cast<int>(seed), junctionCount})) {
        SKIP("a zero-area triangle in the union");
    }
    const Cases::Survey survey = Cases::toSurvey(Cases::randomWild(seed, junctionCount));
    const cwPassageTubeMesh mesh = cwPassageTubeBuilder::build(survey.region, survey.positionsByCave, {}, cwPassageTubeBuilder::Options());
    const QVector<cwPassageTubeMeshCheck::Issue> issues = cwPassageTubeMeshCheck::degenerateTriangles(mesh, cwPassageTubeMeshCheck::wholeMesh(mesh));
    const auto degenerate = std::count_if(issues.cbegin(), issues.cend(), [](const cwPassageTubeMeshCheck::Issue& issue) {
        return issue.type == cwPassageTubeMeshCheck::DegenerateTriangle;
    });
    INFO(cwPassageTubeMeshCheck::toText({issues, 0, {}}).toStdString());
    CHECK(degenerate == 0);
}

// Uniform bearings and independent readings make hairpin cones (Crease,
// SliverTriangle, SelfIntersection), radius jumps between samples
// (RadiusJump) and walls off a ring that a station's facing tilts
// (WallMissed).
TEST_CASE("cwPassageTubeCases wild random surveys build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    const auto [seed, junctions] = randomSurvey(GENERATE(range(1, kRandomSeedCount + 1)));
    const int junctionCount = GENERATE_COPY(0, junctions);
    INFO("seed " << seed << ", " << junctionCount << " junctions");
    checkBuildsClean(Cases::randomWild(seed, junctionCount));
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
        const Cases::Trip wild = Cases::randomWild(seed, junctions);
        CHECK(wild == Cases::randomWild(seed, junctions));
        CHECK(junctionCount(wild) == junctions);
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
        CHECK(Cases::galleryCases().size() == 21);
        CHECK(caseNames([](const Cases::Case& entry) { return entry.tags.contains(QStringLiteral("bend")); }).size() == 4);
    }
}
