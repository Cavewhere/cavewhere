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
constexpr double kAngleToleranceDegrees = 1e-3;
constexpr int kRandomSeedCount = 50;
constexpr int kMaxRandomJunctions = 6;
constexpr float kClearanceToleranceMeters = 1e-4f;
// Readings print with six significant digits.
constexpr double kLrudStepTolerance = 1e-4;
// A station's readings in Cases::Station::lrud order.
constexpr int kLeftReading = 0;
constexpr int kRightReading = 1;
constexpr int kUpReading = 2;
constexpr int kDownReading = 3;

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
    // The a6 -> a9 shaft is a box padded off both stations (T07am): its cap
    // at a6 slides back into chunk 2's passage and folds the box's rings
    // behind it, the union leaves slivers where the box meets the passages
    // at a6 and a9, the box's walls crease against its cap at a9 (one crease
    // lies nearer a10), and its padded ring leaves a6's side walls off the
    // tube.
    {QStringLiteral("a6"), cwPassageTubeMeshCheck::FoldBack},
    {QStringLiteral("a6"), cwPassageTubeMeshCheck::SliverTriangle},
    {QStringLiteral("a6"), cwPassageTubeMeshCheck::WallMissed},
    {QStringLiteral("a9"), cwPassageTubeMeshCheck::FoldBack},
    {QStringLiteral("a9"), cwPassageTubeMeshCheck::SliverTriangle},
    {QStringLiteral("a9"), cwPassageTubeMeshCheck::Crease},
    {QStringLiteral("a10"), cwPassageTubeMeshCheck::Crease},
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

// Every case the union still leaves unclean, with its issue counts.
// - 8, 9 and 10's plumb legs are boxes padded off their stations (T07am):
//   their rings are a few meters tall over a 10 m or longer drop, so each
//   box is a sheared slab whose walls crease against its caps, the padded
//   rings leave the stations' readings off the tube, the shot runs out of
//   the slab, and 9's caps slide back along the slab and fold its rings.
// - 21's small arm is its own chain, whose cap slides along it into the
//   room and folds the rings behind it (T07al).
// - 20's tie-in turns back past 150°, and the two trips' chains end there
//   square to their own legs, so the outside of the bend is their caps.
// - 23 and the miter sweep's 150 and 170° bends pinch to the hairpin's
//   sheet at their station (redesign step 1, accepted), which folds the
//   walls against each other.
// - 25's shaft and the pitch sweep's 90 and 80° pitches are boxes padded off
//   their stations (T07am), whose walls crease against the caps at the
//   bottom; 25's foot turns 120° in plan, so the leg out of it crosses the
//   box.
// - The sweep, 710 of its 1836 junction cases, in overlapping families:
//   arms 2.5 times the through's size (466), arms sloping 60° (349),
//   throughs bent 90° at J (264), arms within 20° of the through in plan
//   (131), plumb arms, which are boxes padded off their stations, and arms
//   as big as the through in threes. A slid cap folds the rings behind it
//   (FoldBack), walls fold against each other or cross (Crease,
//   SelfIntersection, SliverTriangle), or a cap shows past the passages it
//   meets (CapExposed).
// - Random surveys, named by seed and junction count: a cap slid back along
//   its leg folds the rings behind it (FoldBack) and leaves walls off the
//   tube (WallMissed), a slid cap or a sharp bend folds against a wall
//   (Crease), walls that touch along a line leave slivers (SliverTriangle),
//   and 32's 78° leg runs 2.6 m in plan over 12.8 m, so the inside wall of
//   the bend before it reaches past its vertical rings (SelfIntersection).
// - Wild random surveys 8 and 46 leave a zero-area triangle
//   (DegenerateTriangle) beside a leg 38° to 53° steep whose rings stand
//   vertical (T07am).
const QStringList kUnionOpenCases {
    QStringLiteral("8. Pit dropping into a passage"), // Crease 12, WallMissed 4, CenterlineOutside 1
    QStringLiteral("9. Passage joining a shaft"), // Crease 17, FoldBack 9, WallMissed 6, CenterlineOutside 2, CapExposed 1
    QStringLiteral("10. Passage turning straight down a pit, side arm at the lip"), // Crease 8, WallMissed 3, CenterlineOutside 1
    QStringLiteral("21. Small arm along the wall of a big room"), // FoldBack 1, WallMissed 2
    QStringLiteral("20. Passage surveyed over two trips with a hairpin tie-in and a re-shot leg"), // CapExposed 1
    QStringLiteral("23. Sharp bend at a station reading zero on the outside wall"), // Crease 6, SelfIntersection 7, FoldBack 1
    QStringLiteral("25. Zero floor reading at the foot of a shaft"), // Crease 8, SelfIntersection 26, FoldBack 10, WallBeyondReading 1, CapExposed 2
    QStringLiteral("31. Bend of 150 degrees mitered on one ring"), // Crease 4
    QStringLiteral("31. Bend of 170 degrees mitered on one ring"), // Crease 10, SelfIntersection 81, FoldBack 1
    QStringLiteral("38. Pitch of 90 degrees below a level lead-in"), // Crease 5, SliverTriangle 5, WallMissed 3
    QStringLiteral("38. Pitch of 80 degrees below a level lead-in"), // Crease 3

    QStringLiteral("plan 90, profile 0, ratio 1, 1 arm, bend 90, same section"), // SliverTriangle 25
    QStringLiteral("plan 90, profile 0, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 1, SliverTriangle 45, DegenerateTriangle 4, SelfIntersection 1, FoldBack 9
    QStringLiteral("plan 90, profile 0, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // FoldBack 5
    QStringLiteral("plan 90, profile 0, ratio 1, 2 arms same side, bend 90, same section"), // SliverTriangle 7
    QStringLiteral("plan 90, profile 0, ratio 1, 3 arms, bend 90, same section"), // SliverTriangle 11
    QStringLiteral("plan 90, profile 0, ratio 2.5, 2 arms opposite, bend 40, same section"), // FoldBack 1
    QStringLiteral("plan 90, profile 0, ratio 2.5, 2 arms same side, bend 0, same section"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 90, profile 0, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 90, profile 30, ratio 1, 1 arm, bend 90, same section"), // Crease 3, SliverTriangle 39, WallMissed 2
    QStringLiteral("plan 90, profile 30, ratio 1, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 90, profile 30, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 14, SliverTriangle 65, DegenerateTriangle 2, SelfIntersection 91, FoldBack 10, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 2, SliverTriangle 1, FoldBack 6, WallMissed 2, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 1, 2 arms same side, bend 90, same section"), // Crease 10, SliverTriangle 39, SelfIntersection 60
    QStringLiteral("plan 90, profile 30, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 2, SelfIntersection 8
    QStringLiteral("plan 90, profile 30, ratio 1, 3 arms, bend 40, section x2 past J"), // FoldBack 3
    QStringLiteral("plan 90, profile 30, ratio 1, 3 arms, bend 90, same section"), // Crease 13, SliverTriangle 42, SelfIntersection 69, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 5, SelfIntersection 3, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 1 arm, bend 40, same section"), // WallMissed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms opposite, bend 40, same section"), // FoldBack 7, WallMissed 2
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // FoldBack 3
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 2, SelfIntersection 33, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // SelfIntersection 29, FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 1, SelfIntersection 13, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // SelfIntersection 12, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 1, SelfIntersection 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 1, SelfIntersection 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile 30, ratio 2.5, 3 arms, bend 0, same section"), // Crease 12, SelfIntersection 60, FoldBack 18
    QStringLiteral("plan 90, profile 30, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 8, SelfIntersection 49, FoldBack 16
    QStringLiteral("plan 90, profile 30, ratio 2.5, 3 arms, bend 40, same section"), // Crease 5, SelfIntersection 35, FoldBack 9
    QStringLiteral("plan 90, profile 30, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 5, SliverTriangle 4, SelfIntersection 79, FoldBack 19
    QStringLiteral("plan 90, profile 30, ratio 2.5, 3 arms, bend 90, same section"), // Crease 3, SelfIntersection 48, FoldBack 14, WallMissed 2
    QStringLiteral("plan 90, profile 30, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 2, SelfIntersection 44, FoldBack 9, WallMissed 2
    QStringLiteral("plan 90, profile 60, ratio 1, 1 arm, bend 90, same section"), // Crease 3, SliverTriangle 31, DegenerateTriangle 1, WallMissed 1
    QStringLiteral("plan 90, profile 60, ratio 1, 1 arm, bend 90, section x2 past J"), // Crease 1, WallMissed 1
    QStringLiteral("plan 90, profile 60, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 19, SliverTriangle 58, DegenerateTriangle 2, SelfIntersection 65, FoldBack 10, CenterlineOutside 1, CapExposed 2
    QStringLiteral("plan 90, profile 60, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 10, SliverTriangle 1, SelfIntersection 39, FoldBack 8, WallMissed 2, CapExposed 1
    QStringLiteral("plan 90, profile 60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 13, SliverTriangle 34, DegenerateTriangle 2, SelfIntersection 70, CenterlineOutside 1, CapExposed 1
    QStringLiteral("plan 90, profile 60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 6, SelfIntersection 30, WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile 60, ratio 1, 3 arms, bend 40, section x2 past J"), // FoldBack 6
    QStringLiteral("plan 90, profile 60, ratio 1, 3 arms, bend 90, same section"), // Crease 9, SliverTriangle 36, SelfIntersection 95, FoldBack 2, CapExposed 1
    QStringLiteral("plan 90, profile 60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 1, SelfIntersection 13, FoldBack 2, CapExposed 1
    QStringLiteral("plan 90, profile 60, ratio 2.5, 1 arm, bend 40, same section"), // WallMissed 1
    QStringLiteral("plan 90, profile 60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 1, SelfIntersection 13, FoldBack 8, WallMissed 2, CapExposed 1
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // FoldBack 6, WallMissed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 13, SelfIntersection 29, FoldBack 17, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 8, SelfIntersection 38, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 6, SelfIntersection 54, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 3, SelfIntersection 35, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 4, SelfIntersection 17, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 4, SelfIntersection 17, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 44, DegenerateTriangle 4, SliverTriangle 1, SelfIntersection 87, FoldBack 18, WallMissed 4
    QStringLiteral("plan 90, profile 60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 26, SliverTriangle 2, SelfIntersection 34, FoldBack 18, WallMissed 4
    QStringLiteral("plan 90, profile 60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 27, SliverTriangle 1, SelfIntersection 29, FoldBack 9
    QStringLiteral("plan 90, profile 60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 29, SliverTriangle 8, DegenerateTriangle 4, SelfIntersection 72, FoldBack 19
    QStringLiteral("plan 90, profile 60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 23, SelfIntersection 80, FoldBack 16, WallMissed 2
    QStringLiteral("plan 90, profile 60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 23, SliverTriangle 1, SelfIntersection 66, FoldBack 14, WallMissed 2
    QStringLiteral("plan 90, profile 90, ratio 0.3, 1 arm, bend 0, same section"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 0.3, 1 arm, bend 0, section x2 past J"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 0.3, 1 arm, bend 40, same section"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 0.3, 1 arm, bend 40, section x2 past J"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 0.3, 1 arm, bend 90, same section"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 0.3, 1 arm, bend 90, section x2 past J"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 1, 1 arm, bend 0, same section"), // Crease 6, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 1, 1 arm, bend 0, section x2 past J"), // Crease 6, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 1, 1 arm, bend 40, same section"), // Crease 13, SelfIntersection 107, FoldBack 10, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 13, SelfIntersection 106, FoldBack 10, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 1, 1 arm, bend 90, same section"), // Crease 14, SelfIntersection 9, FoldBack 10, WallMissed 4, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 1, 1 arm, bend 90, section x2 past J"), // Crease 11, FoldBack 10, WallMissed 4, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 2.5, 1 arm, bend 0, same section"), // Crease 10, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 10, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 2.5, 1 arm, bend 40, same section"), // Crease 15, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 9, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 2.5, 1 arm, bend 90, same section"), // Crease 14, SelfIntersection 43, FoldBack 8, WallMissed 5, CenterlineOutside 1
    QStringLiteral("plan 90, profile 90, ratio 2.5, 1 arm, bend 90, section x2 past J"), // Crease 11, SelfIntersection 9, FoldBack 6, WallMissed 5, CenterlineOutside 1
    QStringLiteral("plan 90, profile -30, ratio 1, 1 arm, bend 90, same section"), // Crease 2, SliverTriangle 46, DegenerateTriangle 1, WallMissed 1, CenterlineOutside 1
    QStringLiteral("plan 90, profile -30, ratio 1, 1 arm, bend 90, section x2 past J"), // WallMissed 2
    QStringLiteral("plan 90, profile -30, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 9, SliverTriangle 31, DegenerateTriangle 2, SelfIntersection 85, FoldBack 10, CenterlineOutside 1
    QStringLiteral("plan 90, profile -30, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 3, SliverTriangle 2, FoldBack 6, WallMissed 2, CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 1, 2 arms same side, bend 90, same section"), // Crease 11, SliverTriangle 35, SelfIntersection 95, CenterlineOutside 1
    QStringLiteral("plan 90, profile -30, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 90, profile -30, ratio 1, 3 arms, bend 40, section x2 past J"), // FoldBack 3
    QStringLiteral("plan 90, profile -30, ratio 1, 3 arms, bend 90, same section"), // Crease 14, SliverTriangle 39, DegenerateTriangle 2, SelfIntersection 132, CenterlineOutside 1, CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 1, 3 arms, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms opposite, bend 40, same section"), // FoldBack 7
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // FoldBack 3
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 1, SelfIntersection 6, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms same side, bend 40, same section"), // CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms same side, bend 90, same section"), // WallMissed 1, CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 90, profile -30, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 1, SelfIntersection 40, FoldBack 10, WallMissed 2, CapExposed 1
    QStringLiteral("plan 90, profile -60, ratio 1, 1 arm, bend 90, same section"), // Crease 4, SliverTriangle 44, DegenerateTriangle 1, SelfIntersection 58, WallMissed 1, CenterlineOutside 1
    QStringLiteral("plan 90, profile -60, ratio 1, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 11, SliverTriangle 27, SelfIntersection 55, FoldBack 10, CenterlineOutside 1, CapExposed 1
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 8, SelfIntersection 35, FoldBack 8, WallMissed 2, CapExposed 1
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms same side, bend 0, same section"), // Crease 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms same side, bend 0, section x2 past J"), // Crease 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 13, SliverTriangle 40, SelfIntersection 95, CenterlineOutside 1, CapExposed 1
    QStringLiteral("plan 90, profile -60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 9, SelfIntersection 70, WallMissed 1, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 2
    QStringLiteral("plan 90, profile -60, ratio 1, 3 arms, bend 40, section x2 past J"), // FoldBack 6
    QStringLiteral("plan 90, profile -60, ratio 1, 3 arms, bend 90, same section"), // Crease 14, SliverTriangle 30, SelfIntersection 45, CenterlineOutside 1, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 7, SliverTriangle 2, SelfIntersection 68, CapExposed 1
    QStringLiteral("plan 90, profile -60, ratio 2.5, 1 arm, bend 40, same section"), // WallMissed 1
    QStringLiteral("plan 90, profile -60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 2, SliverTriangle 1, SelfIntersection 18, FoldBack 8, WallMissed 2, CapExposed 1
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // FoldBack 6, WallMissed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 17, SelfIntersection 35, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 10, SelfIntersection 37, FoldBack 14, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 5, SelfIntersection 38, FoldBack 14, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 5, SelfIntersection 24, FoldBack 14, WallMissed 2, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 8, SelfIntersection 13, FoldBack 14, WallMissed 1, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 8, SelfIntersection 13, FoldBack 14, WallMissed 1, CapExposed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 13, DegenerateTriangle 2, SelfIntersection 254, FoldBack 18, WallMissed 4
    QStringLiteral("plan 90, profile -60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 11, SelfIntersection 139, FoldBack 18, WallMissed 4
    QStringLiteral("plan 90, profile -60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 8, SelfIntersection 102, FoldBack 9
    QStringLiteral("plan 90, profile -60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 14, SliverTriangle 2, SelfIntersection 113, FoldBack 19
    QStringLiteral("plan 90, profile -60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 12, SelfIntersection 42, FoldBack 16, WallMissed 2
    QStringLiteral("plan 90, profile -60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 11, SelfIntersection 18, FoldBack 13, WallMissed 2
    QStringLiteral("plan 90, profile -90, ratio 0.3, 1 arm, bend 0, same section"), // Crease 28, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 0.3, 1 arm, bend 0, section x2 past J"), // Crease 27, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 0.3, 1 arm, bend 40, same section"), // Crease 27, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 0.3, 1 arm, bend 40, section x2 past J"), // Crease 27, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 0.3, 1 arm, bend 90, same section"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 0.3, 1 arm, bend 90, section x2 past J"), // Crease 26, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 1, 1 arm, bend 0, same section"), // Crease 8, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 1, 1 arm, bend 0, section x2 past J"), // Crease 8, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 1, 1 arm, bend 40, same section"), // Crease 13, SelfIntersection 7, FoldBack 10, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 12, SliverTriangle 2, SelfIntersection 7, FoldBack 10, WallMissed 3, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 1, 1 arm, bend 90, same section"), // Crease 14, SelfIntersection 135, FoldBack 10, WallMissed 4, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 1, 1 arm, bend 90, section x2 past J"), // Crease 14, SliverTriangle 4, SelfIntersection 141, FoldBack 10, WallMissed 4, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 2.5, 1 arm, bend 0, same section"), // Crease 10, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 10, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 2.5, 1 arm, bend 40, same section"), // Crease 14, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 10, WallMissed 6, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 2.5, 1 arm, bend 90, same section"), // Crease 17, SelfIntersection 27, FoldBack 8, WallMissed 5, CenterlineOutside 1
    QStringLiteral("plan 90, profile -90, ratio 2.5, 1 arm, bend 90, section x2 past J"), // Crease 12, SelfIntersection 16, FoldBack 6, WallMissed 5, CenterlineOutside 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 0, same section"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 40, same section"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 40, section x2 past J"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 90, same section"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 1 arm, bend 90, section x2 past J"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms opposite, bend 90, same section"), // FoldBack 8
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // FoldBack 7
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms same side, bend 0, same section"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms same side, bend 40, same section"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms same side, bend 90, same section"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // FoldBack 4, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 0, ratio 2.5, 3 arms, bend 0, section x2 past J"), // SliverTriangle 1
    QStringLiteral("plan 60, profile 30, ratio 1, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 60, profile 30, ratio 1, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 1, 3 arms, bend 40, same section"), // Crease 11, DegenerateTriangle 4, SliverTriangle 44, SelfIntersection 87
    QStringLiteral("plan 60, profile 30, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 13, DegenerateTriangle 2, SliverTriangle 40, SelfIntersection 96
    QStringLiteral("plan 60, profile 30, ratio 1, 3 arms, bend 90, same section"), // Crease 15, SliverTriangle 41, DegenerateTriangle 4, SelfIntersection 91, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 12, SliverTriangle 42, DegenerateTriangle 3, SelfIntersection 81, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 1 arm, bend 0, same section"), // Crease 1, FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 1, FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 1 arm, bend 40, same section"), // FoldBack 5, WallMissed 2
    QStringLiteral("plan 60, profile 30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 1, FoldBack 5, WallMissed 2
    QStringLiteral("plan 60, profile 30, ratio 2.5, 1 arm, bend 90, same section"), // Crease 1, FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 1 arm, bend 90, section x2 past J"), // Crease 1, FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 1, FoldBack 10
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 1, FoldBack 10
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms opposite, bend 40, same section"), // FoldBack 10, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 1, FoldBack 10, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 2, SelfIntersection 13, FoldBack 14
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 3, SelfIntersection 8, FoldBack 12
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 1, FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 1, FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 1, SelfIntersection 51, FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 2, SelfIntersection 13, FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 1, SliverTriangle 1, FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 1, FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 3 arms, bend 0, same section"), // Crease 1, FoldBack 10
    QStringLiteral("plan 60, profile 30, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 1, FoldBack 10, WallMissed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 3 arms, bend 40, same section"), // Crease 8, SliverTriangle 3, SelfIntersection 44, FoldBack 18
    QStringLiteral("plan 60, profile 30, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 4, SelfIntersection 12, FoldBack 16
    QStringLiteral("plan 60, profile 30, ratio 2.5, 3 arms, bend 90, same section"), // Crease 2, SelfIntersection 24, FoldBack 14, WallMissed 1
    QStringLiteral("plan 60, profile 30, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 2, SelfIntersection 16, FoldBack 12, WallMissed 1
    QStringLiteral("plan 60, profile 60, ratio 0.3, 2 arms opposite, bend 90, section x2 past J"), // SliverTriangle 1
    QStringLiteral("plan 60, profile 60, ratio 0.3, 2 arms same side, bend 0, same section"), // Crease 1
    QStringLiteral("plan 60, profile 60, ratio 0.3, 2 arms same side, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 60, profile 60, ratio 0.3, 3 arms, bend 90, section x2 past J"), // SliverTriangle 1
    QStringLiteral("plan 60, profile 60, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 60, profile 60, ratio 1, 1 arm, bend 90, section x2 past J"), // Crease 2
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms opposite, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 1, FoldBack 2
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 2
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms same side, bend 0, same section"), // Crease 1
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 1, FoldBack 2
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 1, CapExposed 1
    QStringLiteral("plan 60, profile 60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 3
    QStringLiteral("plan 60, profile 60, ratio 1, 3 arms, bend 0, same section"), // Crease 1
    QStringLiteral("plan 60, profile 60, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 5
    QStringLiteral("plan 60, profile 60, ratio 1, 3 arms, bend 40, same section"), // Crease 28, SliverTriangle 80, SelfIntersection 304, CenterlineOutside 1
    QStringLiteral("plan 60, profile 60, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 27, SliverTriangle 70, SelfIntersection 293, CenterlineOutside 1
    QStringLiteral("plan 60, profile 60, ratio 1, 3 arms, bend 90, same section"), // Crease 28, DegenerateTriangle 5, SliverTriangle 66, SelfIntersection 234, FoldBack 2, CapExposed 1
    QStringLiteral("plan 60, profile 60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 29, SliverTriangle 60, DegenerateTriangle 1, SelfIntersection 230, CapExposed 1
    QStringLiteral("plan 60, profile 60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 3, SelfIntersection 12, FoldBack 7, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 2, SelfIntersection 12, FoldBack 7, WallMissed 1
    QStringLiteral("plan 60, profile 60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 10, SelfIntersection 16, FoldBack 7, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 1, SelfIntersection 11, FoldBack 7, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 1 arm, bend 90, same section"), // Crease 5, SelfIntersection 9, FoldBack 7, WallMissed 1
    QStringLiteral("plan 60, profile 60, ratio 2.5, 1 arm, bend 90, section x2 past J"), // Crease 5, SelfIntersection 9, FoldBack 7, WallMissed 1
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 7, SelfIntersection 18, FoldBack 14, WallMissed 4, CapExposed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 5, SelfIntersection 20, FoldBack 14, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 16, SelfIntersection 29, FoldBack 14, WallMissed 3, CapExposed 1
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 6, DegenerateTriangle 2, SelfIntersection 24, FoldBack 14, WallMissed 3, CapExposed 1
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 23, SliverTriangle 1, SelfIntersection 54, FoldBack 16, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 10, SliverTriangle 1, SelfIntersection 38, FoldBack 16, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 12, SelfIntersection 25, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 8, SelfIntersection 8, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 4, SelfIntersection 37, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 8, DegenerateTriangle 2, SelfIntersection 26, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 14, SliverTriangle 1, SelfIntersection 8, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 8, DegenerateTriangle 2, SliverTriangle 3, SelfIntersection 8, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 7, SelfIntersection 18, FoldBack 14
    QStringLiteral("plan 60, profile 60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 5, SelfIntersection 20, FoldBack 14
    QStringLiteral("plan 60, profile 60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 26, SelfIntersection 46, FoldBack 23, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 12, DegenerateTriangle 2, SelfIntersection 31, FoldBack 22, WallMissed 2
    QStringLiteral("plan 60, profile 60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 23, SliverTriangle 1, SelfIntersection 73, FoldBack 16, WallMissed 1
    QStringLiteral("plan 60, profile 60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 10, SliverTriangle 2, SelfIntersection 36, FoldBack 16, WallMissed 1
    QStringLiteral("plan 60, profile -30, ratio 0.3, 2 arms opposite, bend 90, section x2 past J"), // SliverTriangle 1
    QStringLiteral("plan 60, profile -30, ratio 0.3, 3 arms, bend 90, section x2 past J"), // SliverTriangle 1
    QStringLiteral("plan 60, profile -30, ratio 1, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 60, profile -30, ratio 1, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 1, 2 arms same side, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 1, 3 arms, bend 40, same section"), // Crease 10, SliverTriangle 43, SelfIntersection 115
    QStringLiteral("plan 60, profile -30, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 10, SliverTriangle 46, SelfIntersection 111
    QStringLiteral("plan 60, profile -30, ratio 1, 3 arms, bend 90, same section"), // Crease 7, DegenerateTriangle 3, SliverTriangle 39, SelfIntersection 92, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 9, SliverTriangle 39, SelfIntersection 98, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 1 arm, bend 0, same section"), // FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 1 arm, bend 40, same section"), // Crease 1, FoldBack 5, WallMissed 2
    QStringLiteral("plan 60, profile -30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // FoldBack 5, WallMissed 2
    QStringLiteral("plan 60, profile -30, ratio 2.5, 1 arm, bend 90, same section"), // FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 1 arm, bend 90, section x2 past J"), // FoldBack 5, WallMissed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 1, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 2, SelfIntersection 9, FoldBack 9
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 1, SelfIntersection 8, FoldBack 7
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms same side, bend 0, same section"), // FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms same side, bend 40, same section"), // FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms same side, bend 90, same section"), // FoldBack 5, WallMissed 2, CapExposed 2
    QStringLiteral("plan 60, profile -30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // FoldBack 5, WallMissed 1, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 1, 1 arm, bend 90, section x2 past J"), // Crease 1, SliverTriangle 1
    QStringLiteral("plan 60, profile -60, ratio 1, 2 arms opposite, bend 90, same section"), // FoldBack 2
    QStringLiteral("plan 60, profile -60, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 1
    QStringLiteral("plan 60, profile -60, ratio 1, 2 arms same side, bend 0, same section"), // Crease 4, SelfIntersection 1
    QStringLiteral("plan 60, profile -60, ratio 1, 2 arms same side, bend 0, section x2 past J"), // Crease 4, SelfIntersection 1
    QStringLiteral("plan 60, profile -60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 1
    QStringLiteral("plan 60, profile -60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 2, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 2
    QStringLiteral("plan 60, profile -60, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 60, profile -60, ratio 1, 3 arms, bend 40, same section"), // Crease 18, SliverTriangle 52, SelfIntersection 57, CenterlineOutside 1
    QStringLiteral("plan 60, profile -60, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 16, SliverTriangle 43, SelfIntersection 60, CenterlineOutside 1
    QStringLiteral("plan 60, profile -60, ratio 1, 3 arms, bend 90, same section"), // Crease 13, SliverTriangle 32, SelfIntersection 63, CenterlineOutside 1, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 14, SliverTriangle 32, SelfIntersection 71, CenterlineOutside 1, CapExposed 1
    QStringLiteral("plan 60, profile -60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 5, SelfIntersection 12, FoldBack 7, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 7, SelfIntersection 6, FoldBack 7, WallMissed 1
    QStringLiteral("plan 60, profile -60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 11, SelfIntersection 11, FoldBack 7, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 4, SelfIntersection 12, FoldBack 7, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 1 arm, bend 90, same section"), // Crease 6, SelfIntersection 7, FoldBack 7, WallMissed 1
    QStringLiteral("plan 60, profile -60, ratio 2.5, 1 arm, bend 90, section x2 past J"), // Crease 6, SelfIntersection 7, FoldBack 7, WallMissed 1
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 2, SelfIntersection 23, FoldBack 14, WallMissed 4, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 4, SliverTriangle 1, SelfIntersection 21, FoldBack 14, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 11, SelfIntersection 27, FoldBack 14, WallMissed 3, CapExposed 1
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 5, SelfIntersection 27, FoldBack 14, WallMissed 3, CapExposed 1
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 25, SelfIntersection 88, FoldBack 16, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 10, SelfIntersection 19, FoldBack 16, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 17, SliverTriangle 1, SelfIntersection 26, FoldBack 14, WallMissed 2, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 13, SelfIntersection 9, FoldBack 14, WallMissed 1, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 8, SelfIntersection 47, FoldBack 14, WallMissed 2, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 11, SelfIntersection 31, FoldBack 14, WallMissed 2, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 14, SelfIntersection 5, FoldBack 14, WallMissed 1, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 10, SelfIntersection 5, FoldBack 14, WallMissed 1, CapExposed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 5, SelfIntersection 38, FoldBack 14
    QStringLiteral("plan 60, profile -60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 6, SelfIntersection 21, FoldBack 14, WallMissed 1
    QStringLiteral("plan 60, profile -60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 10, SliverTriangle 1, SelfIntersection 127, FoldBack 23, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 10, SelfIntersection 69, FoldBack 22, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 11, SliverTriangle 4, SelfIntersection 27, FoldBack 16, WallMissed 2
    QStringLiteral("plan 60, profile -60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 10, SliverTriangle 3, SelfIntersection 23, FoldBack 16, WallMissed 2
    QStringLiteral("plan 45, profile 0, ratio 1, 2 arms same side, bend 90, same section"), // SliverTriangle 24
    QStringLiteral("plan 45, profile 0, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 2
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 0, same section"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 40, same section"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 40, section x2 past J"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 1 arm, bend 90, same section"), // FoldBack 7
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms opposite, bend 90, same section"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // FoldBack 4, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms same side, bend 0, same section"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms same side, bend 40, same section"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 0, ratio 2.5, 2 arms same side, bend 90, same section"), // FoldBack 7
    QStringLiteral("plan 45, profile 30, ratio 1, 2 arms same side, bend 40, same section"), // SelfIntersection 4
    QStringLiteral("plan 45, profile 30, ratio 1, 2 arms same side, bend 90, same section"), // Crease 1, SliverTriangle 49, SelfIntersection 1
    QStringLiteral("plan 45, profile 30, ratio 1, 3 arms, bend 0, section x2 past J"), // FoldBack 1
    QStringLiteral("plan 45, profile 30, ratio 1, 3 arms, bend 40, same section"), // Crease 2, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 1, 3 arms, bend 90, same section"), // Crease 2, SelfIntersection 1, FoldBack 2
    QStringLiteral("plan 45, profile 30, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 2, SelfIntersection 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 1 arm, bend 0, same section"), // Crease 1, SelfIntersection 12, FoldBack 7, WallMissed 2
    QStringLiteral("plan 45, profile 30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 1, SelfIntersection 12, FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 1 arm, bend 40, same section"), // Crease 1, SelfIntersection 10, FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 1, SelfIntersection 19, FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 1 arm, bend 90, same section"), // Crease 1, SelfIntersection 21, FoldBack 7
    QStringLiteral("plan 45, profile 30, ratio 2.5, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 3, SliverTriangle 1, SelfIntersection 18, FoldBack 14, WallMissed 2
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 3, SliverTriangle 1, SelfIntersection 18, FoldBack 14
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 2, SliverTriangle 1, SelfIntersection 20, FoldBack 14
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 2, SelfIntersection 29, FoldBack 14, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 2, SelfIntersection 32, FoldBack 14
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 2, SelfIntersection 6, FoldBack 7
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 1, SelfIntersection 25, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 1, SelfIntersection 13, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 1, SelfIntersection 11, FoldBack 7, WallMissed 2, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 4, SliverTriangle 1, SelfIntersection 50, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 1, SelfIntersection 22, FoldBack 7, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile 30, ratio 2.5, 3 arms, bend 0, same section"), // Crease 14, SliverTriangle 1, SelfIntersection 178, FoldBack 17
    QStringLiteral("plan 45, profile 30, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 8, SliverTriangle 2, SelfIntersection 153, FoldBack 16, WallMissed 2
    QStringLiteral("plan 45, profile 30, ratio 2.5, 3 arms, bend 40, same section"), // Crease 9, SliverTriangle 6, SelfIntersection 77, FoldBack 17
    QStringLiteral("plan 45, profile 30, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 18, SliverTriangle 9, SelfIntersection 228, FoldBack 8
    QStringLiteral("plan 45, profile 30, ratio 2.5, 3 arms, bend 90, same section"), // Crease 13, SliverTriangle 6, SelfIntersection 117, FoldBack 17
    QStringLiteral("plan 45, profile 30, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 10, DegenerateTriangle 2, SliverTriangle 6, SelfIntersection 116, FoldBack 9
    QStringLiteral("plan 45, profile 60, ratio 1, 2 arms same side, bend 0, same section"), // CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 1, 2 arms same side, bend 0, section x2 past J"), // Crease 1, CapExposed 1
    QStringLiteral("plan 45, profile 60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 3, SelfIntersection 13, CapExposed 1
    QStringLiteral("plan 45, profile 60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 1, SliverTriangle 41, SelfIntersection 98, FoldBack 4, CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 1, 3 arms, bend 40, same section"), // SelfIntersection 15, CapExposed 1
    QStringLiteral("plan 45, profile 60, ratio 1, 3 arms, bend 90, same section"), // FoldBack 6
    QStringLiteral("plan 45, profile 60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 7, SelfIntersection 10, FoldBack 9, WallMissed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 9, SelfIntersection 6, FoldBack 9, WallMissed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 8, SelfIntersection 3, FoldBack 9, WallMissed 1
    QStringLiteral("plan 45, profile 60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 9, SelfIntersection 3, FoldBack 9, WallMissed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 1 arm, bend 90, same section"), // Crease 2, SelfIntersection 22, FoldBack 9
    QStringLiteral("plan 45, profile 60, ratio 2.5, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 13, SelfIntersection 10, FoldBack 18, WallMissed 4, CapExposed 1
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 16, SelfIntersection 10, FoldBack 18, WallMissed 4
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 15, SelfIntersection 11, FoldBack 18, WallMissed 1
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 16, SelfIntersection 7, FoldBack 18, WallMissed 3, CapExposed 1
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 9, SelfIntersection 33, FoldBack 18
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 7, SelfIntersection 14, FoldBack 9, WallMissed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 14, SelfIntersection 39, FoldBack 17, WallMissed 2, CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 13, SelfIntersection 5, FoldBack 17, WallMissed 2, CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 10, SelfIntersection 5, FoldBack 17, WallMissed 1, CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 12, SliverTriangle 1, SelfIntersection 42, FoldBack 17, WallMissed 2, CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 6, DegenerateTriangle 2, SliverTriangle 2, SelfIntersection 13, FoldBack 17, CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 4, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 45, profile 60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 36, SliverTriangle 1, SelfIntersection 152, FoldBack 23, WallMissed 4
    QStringLiteral("plan 45, profile 60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 32, DegenerateTriangle 2, SelfIntersection 62, FoldBack 23, WallMissed 4
    QStringLiteral("plan 45, profile 60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 23, SliverTriangle 1, SelfIntersection 23, FoldBack 24, WallMissed 1
    QStringLiteral("plan 45, profile 60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 22, SliverTriangle 6, SelfIntersection 3, FoldBack 18, WallMissed 4
    QStringLiteral("plan 45, profile 60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 33, SliverTriangle 2, DegenerateTriangle 2, SelfIntersection 138, FoldBack 19
    QStringLiteral("plan 45, profile 60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 22, SliverTriangle 7, SelfIntersection 125, FoldBack 10, WallMissed 1
    QStringLiteral("plan 45, profile -30, ratio 1, 2 arms same side, bend 90, same section"), // Crease 2, SliverTriangle 46, DegenerateTriangle 1, CenterlineOutside 1
    QStringLiteral("plan 45, profile -30, ratio 1, 3 arms, bend 0, same section"), // WallMissed 1
    QStringLiteral("plan 45, profile -30, ratio 1, 3 arms, bend 0, section x2 past J"), // FoldBack 1
    QStringLiteral("plan 45, profile -30, ratio 1, 3 arms, bend 40, same section"), // WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 1, 3 arms, bend 40, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 45, profile -30, ratio 1, 3 arms, bend 90, same section"), // Crease 1, FoldBack 1
    QStringLiteral("plan 45, profile -30, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 2, SliverTriangle 1, WallMissed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 1 arm, bend 0, same section"), // Crease 1, FoldBack 7, WallMissed 2
    QStringLiteral("plan 45, profile -30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 1, SelfIntersection 2, FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 1 arm, bend 40, same section"), // Crease 1, SelfIntersection 2, FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 1, FoldBack 7, WallMissed 2
    QStringLiteral("plan 45, profile -30, ratio 2.5, 1 arm, bend 90, same section"), // Crease 7, SelfIntersection 44, FoldBack 7, WallMissed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms opposite, bend 0, same section"), // WallMissed 2
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 1, SelfIntersection 10, FoldBack 7
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // FoldBack 5
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 1, SelfIntersection 2, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 1, SelfIntersection 2, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 1, SelfIntersection 2, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 1, SelfIntersection 2, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 7, SelfIntersection 37, FoldBack 7, WallMissed 1, CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 45, profile -30, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 1, SliverTriangle 1, SelfIntersection 62, FoldBack 9
    QStringLiteral("plan 45, profile -30, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 1, SelfIntersection 65, FoldBack 9
    QStringLiteral("plan 45, profile -60, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 45, profile -60, ratio 1, 2 arms opposite, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 45, profile -60, ratio 1, 2 arms same side, bend 0, same section"), // Crease 1
    QStringLiteral("plan 45, profile -60, ratio 1, 2 arms same side, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 45, profile -60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 4, SelfIntersection 17
    QStringLiteral("plan 45, profile -60, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 2
    QStringLiteral("plan 45, profile -60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 9, SliverTriangle 47, SelfIntersection 116, FoldBack 3, CenterlineOutside 1, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 1, CapExposed 1
    QStringLiteral("plan 45, profile -60, ratio 1, 3 arms, bend 40, same section"), // Crease 6, SelfIntersection 59, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 1, SliverTriangle 1, CapExposed 1
    QStringLiteral("plan 45, profile -60, ratio 1, 3 arms, bend 90, same section"), // Crease 6, SelfIntersection 32, FoldBack 6
    QStringLiteral("plan 45, profile -60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 6, SelfIntersection 32
    QStringLiteral("plan 45, profile -60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 8, SelfIntersection 19, FoldBack 9, WallMissed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 10, SelfIntersection 21, FoldBack 9, WallMissed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 11, SelfIntersection 18, FoldBack 9, WallMissed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 12, SelfIntersection 22, FoldBack 9, WallMissed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 1 arm, bend 90, same section"), // Crease 11, DegenerateTriangle 2, SelfIntersection 61, FoldBack 9, WallMissed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 18, SelfIntersection 35, FoldBack 18, WallMissed 4, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 22, SelfIntersection 32, FoldBack 18, WallMissed 4, CapExposed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 23, SelfIntersection 29, FoldBack 18, WallMissed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 25, SelfIntersection 30, FoldBack 18, WallMissed 3, CapExposed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 23, SliverTriangle 1, SelfIntersection 172, FoldBack 18, WallMissed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 10, SelfIntersection 19, FoldBack 9, WallMissed 2, CapExposed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 15, SelfIntersection 54, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 17, SelfIntersection 19, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 14, SelfIntersection 18, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 18, SelfIntersection 96, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 16, DegenerateTriangle 2, SelfIntersection 60, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 5, FoldBack 14, WallMissed 1, CapExposed 2
    QStringLiteral("plan 45, profile -60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 7, SliverTriangle 1, SelfIntersection 220, FoldBack 22, WallMissed 4
    QStringLiteral("plan 45, profile -60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 12, SliverTriangle 1, SelfIntersection 160, FoldBack 22, WallMissed 5
    QStringLiteral("plan 45, profile -60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 9, SelfIntersection 111, FoldBack 23, WallMissed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 14, SliverTriangle 4, SelfIntersection 337, FoldBack 17, WallMissed 4
    QStringLiteral("plan 45, profile -60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 16, SliverTriangle 9, SelfIntersection 105, FoldBack 10, WallMissed 1
    QStringLiteral("plan 45, profile -60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 11, SliverTriangle 8, DegenerateTriangle 2, SelfIntersection 104, FoldBack 10, WallMissed 1
    QStringLiteral("plan 30, profile 0, ratio 1, 3 arms, bend 90, same section"), // SliverTriangle 36, FoldBack 9
    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 0, same section"), // FoldBack 8, WallMissed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"), // FoldBack 8, WallMissed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 40, same section"), // FoldBack 8, WallMissed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 1 arm, bend 40, section x2 past J"), // SliverTriangle 4, FoldBack 8, WallMissed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms opposite, bend 0, same section"), // SliverTriangle 4
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // SliverTriangle 4
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms opposite, bend 40, same section"), // SliverTriangle 4
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // SliverTriangle 4
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms opposite, bend 90, same section"), // SliverTriangle 4, FoldBack 4, WallMissed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // SliverTriangle 4, WallMissed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms same side, bend 0, same section"), // FoldBack 8, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // FoldBack 8, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms same side, bend 40, same section"), // FoldBack 8, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile 0, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // SliverTriangle 4, FoldBack 8, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 30, profile 30, ratio 1, 2 arms opposite, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 30, profile 30, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 30, profile 30, ratio 1, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 30, profile 30, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 30, profile 30, ratio 1, 3 arms, bend 90, same section"), // Crease 2, SliverTriangle 21, FoldBack 9
    QStringLiteral("plan 30, profile 30, ratio 1, 3 arms, bend 90, section x2 past J"), // FoldBack 6
    QStringLiteral("plan 30, profile 30, ratio 2.5, 1 arm, bend 0, same section"), // Crease 1, SelfIntersection 13, FoldBack 9, WallMissed 2
    QStringLiteral("plan 30, profile 30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 1, SelfIntersection 27, FoldBack 9, WallMissed 2
    QStringLiteral("plan 30, profile 30, ratio 2.5, 1 arm, bend 40, same section"), // SelfIntersection 21, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 4, SliverTriangle 4, SelfIntersection 86, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 4, SelfIntersection 49, FoldBack 18, WallMissed 2, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 3, SelfIntersection 54, FoldBack 18, WallMissed 2, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 3, SelfIntersection 41, FoldBack 18, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 6, DegenerateTriangle 2, SliverTriangle 2, SelfIntersection 118, FoldBack 18, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 3, SelfIntersection 12, FoldBack 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 2, SelfIntersection 12, FoldBack 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 1, SelfIntersection 43, FoldBack 9, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 1, SelfIntersection 35, FoldBack 9, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms same side, bend 40, same section"), // SelfIntersection 22, FoldBack 9, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 4, SliverTriangle 3, SelfIntersection 92, FoldBack 9, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // SelfIntersection 43, CapExposed 1
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 0, same section"), // Crease 14, SliverTriangle 1, SelfIntersection 74, FoldBack 18
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 7, SelfIntersection 58, FoldBack 16
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 40, same section"), // Crease 6, SelfIntersection 54, FoldBack 18
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 8, DegenerateTriangle 2, SliverTriangle 4, SelfIntersection 91, FoldBack 12, CapExposed 2
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 90, same section"), // Crease 4, SelfIntersection 18
    QStringLiteral("plan 30, profile 30, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 4, SelfIntersection 20
    QStringLiteral("plan 30, profile 60, ratio 1, 1 arm, bend 40, same section"), // SliverTriangle 1
    QStringLiteral("plan 30, profile 60, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 2, SliverTriangle 4, SelfIntersection 2
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms opposite, bend 0, same section"), // Crease 1
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms opposite, bend 40, same section"), // Crease 1, SliverTriangle 1
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms opposite, bend 40, section x2 past J"), // Crease 2, SliverTriangle 4, SelfIntersection 2
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms opposite, bend 90, same section"), // FoldBack 2, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // FoldBack 2, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms same side, bend 0, same section"), // Crease 1
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 1, SliverTriangle 1
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 2, SliverTriangle 4, SelfIntersection 2
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 2, FoldBack 4, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 2, FoldBack 4, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 30, profile 60, ratio 1, 3 arms, bend 40, same section"), // SliverTriangle 1
    QStringLiteral("plan 30, profile 60, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 2, SliverTriangle 4
    QStringLiteral("plan 30, profile 60, ratio 1, 3 arms, bend 90, same section"), // Crease 7, SliverTriangle 20, FoldBack 10
    QStringLiteral("plan 30, profile 60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 1, FoldBack 8
    QStringLiteral("plan 30, profile 60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 21, SelfIntersection 62, FoldBack 9, WallMissed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 19, SelfIntersection 54, FoldBack 9, WallMissed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 20, SelfIntersection 37, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 24, SliverTriangle 3, SelfIntersection 93, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 1 arm, bend 90, same section"), // WallMissed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 41, SliverTriangle 6, SelfIntersection 99, FoldBack 18, WallMissed 4, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 37, SliverTriangle 6, SelfIntersection 96, FoldBack 18, WallMissed 4, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 40, SliverTriangle 5, SelfIntersection 79, FoldBack 18, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 42, SliverTriangle 7, SelfIntersection 202, FoldBack 18, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 20, SliverTriangle 3, SelfIntersection 46, FoldBack 9, WallMissed 2, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 21, SliverTriangle 1, SelfIntersection 34, FoldBack 9, WallMissed 2, CapExposed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 24, SliverTriangle 1, SelfIntersection 104, FoldBack 17, WallMissed 2, CapExposed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 26, SliverTriangle 1, SelfIntersection 67, FoldBack 17, WallMissed 2, CapExposed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 27, SelfIntersection 41, FoldBack 17, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 28, SliverTriangle 3, SelfIntersection 95, FoldBack 17, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 5, FoldBack 16, CapExposed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 7, SelfIntersection 52, FoldBack 16, CapExposed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 43, SliverTriangle 1, SelfIntersection 95, FoldBack 18, WallMissed 4
    QStringLiteral("plan 30, profile 60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 25, SelfIntersection 67, FoldBack 18, WallMissed 4
    QStringLiteral("plan 30, profile 60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 30, SliverTriangle 1, SelfIntersection 59, FoldBack 23
    QStringLiteral("plan 30, profile 60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 32, SliverTriangle 2, SelfIntersection 156, FoldBack 15, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile 60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 4, WallMissed 1
    QStringLiteral("plan 30, profile 60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 4, WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 1 arm, bend 0, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 1, SelfIntersection 1, WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms opposite, bend 40, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms opposite, bend 40, section x2 past J"), // Crease 1, SelfIntersection 1, CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms opposite, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms same side, bend 0, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 1, WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 1, 3 arms, bend 90, same section"), // Crease 3, SliverTriangle 17, FoldBack 9
    QStringLiteral("plan 30, profile -30, ratio 1, 3 arms, bend 90, section x2 past J"), // FoldBack 4
    QStringLiteral("plan 30, profile -30, ratio 2.5, 1 arm, bend 0, same section"), // Crease 5, SelfIntersection 10, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 5, SelfIntersection 10, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 1 arm, bend 40, same section"), // Crease 4, SelfIntersection 5, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 7, SliverTriangle 3, SelfIntersection 68, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 0, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 40, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // SelfIntersection 57, CapExposed 2
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 90, same section"), // FoldBack 5, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 5, SelfIntersection 16, FoldBack 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 5, SelfIntersection 14, FoldBack 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 4, SelfIntersection 13, FoldBack 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 7, SliverTriangle 2, SelfIntersection 69, FoldBack 9, WallMissed 1, CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 30, profile -30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // SelfIntersection 52, CapExposed 2
    QStringLiteral("plan 30, profile -30, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 2, SelfIntersection 55, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 1, 1 arm, bend 40, section x2 past J"), // Crease 1, SliverTriangle 4
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms opposite, bend 0, same section"), // Crease 1, FoldBack 2
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms opposite, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms opposite, bend 40, same section"), // Crease 1, CapExposed 1
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms opposite, bend 40, section x2 past J"), // Crease 2, SliverTriangle 4, FoldBack 1, CapExposed 1
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms same side, bend 0, same section"), // Crease 1
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 1, FoldBack 2
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 2, SliverTriangle 4, FoldBack 2
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 4, FoldBack 3, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 3, FoldBack 3, CapExposed 1
    QStringLiteral("plan 30, profile -60, ratio 1, 3 arms, bend 40, same section"), // Crease 1
    QStringLiteral("plan 30, profile -60, ratio 1, 3 arms, bend 40, section x2 past J"), // Crease 1, SliverTriangle 4
    QStringLiteral("plan 30, profile -60, ratio 1, 3 arms, bend 90, same section"), // Crease 8, SliverTriangle 24, FoldBack 10
    QStringLiteral("plan 30, profile -60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 2, FoldBack 8
    QStringLiteral("plan 30, profile -60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 21, SelfIntersection 73, FoldBack 9, WallMissed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 19, SelfIntersection 51, FoldBack 9, WallMissed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 20, SelfIntersection 79, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 26, SliverTriangle 3, SelfIntersection 157, FoldBack 9, WallMissed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 1 arm, bend 90, same section"), // WallMissed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 1 arm, bend 90, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 14, SliverTriangle 3, SelfIntersection 43, FoldBack 18, WallMissed 4, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 7, SliverTriangle 3, SelfIntersection 40, FoldBack 18, WallMissed 4, CapExposed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 12, SliverTriangle 2, SelfIntersection 24, FoldBack 18, CapExposed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 16, SliverTriangle 7, SelfIntersection 83, FoldBack 18, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 12, SliverTriangle 1, SelfIntersection 37, FoldBack 9, WallMissed 2, CapExposed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 9, SliverTriangle 1, SelfIntersection 22, FoldBack 9, WallMissed 2, CapExposed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 27, SelfIntersection 108, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 30, SelfIntersection 122, FoldBack 16, WallMissed 2, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 25, SelfIntersection 90, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 30, SliverTriangle 2, SelfIntersection 152, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 6, FoldBack 14, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 8, SliverTriangle 1, SelfIntersection 46, FoldBack 14, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 12, SelfIntersection 235, FoldBack 18, WallMissed 4
    QStringLiteral("plan 30, profile -60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 11, SelfIntersection 125, FoldBack 18, WallMissed 4
    QStringLiteral("plan 30, profile -60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 13, SelfIntersection 102, FoldBack 22
    QStringLiteral("plan 30, profile -60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 13, SliverTriangle 2, SelfIntersection 146, FoldBack 15, WallMissed 1, CapExposed 2
    QStringLiteral("plan 30, profile -60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 4, SelfIntersection 52, WallMissed 1
    QStringLiteral("plan 30, profile -60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 2, SelfIntersection 55
    QStringLiteral("plan 20, profile 0, ratio 2.5, 1 arm, bend 0, same section"), // FoldBack 9, WallMissed 1
    QStringLiteral("plan 20, profile 0, ratio 2.5, 1 arm, bend 0, section x2 past J"), // FoldBack 9, WallMissed 1
    QStringLiteral("plan 20, profile 0, ratio 2.5, 1 arm, bend 40, same section"), // FoldBack 9
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms opposite, bend 90, same section"), // FoldBack 1
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms same side, bend 0, same section"), // FoldBack 9, CapExposed 1
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // FoldBack 9, CapExposed 1
    QStringLiteral("plan 20, profile 0, ratio 2.5, 2 arms same side, bend 40, same section"), // FoldBack 9, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 0.3, 2 arms opposite, bend 40, same section"), // Crease 3
    QStringLiteral("plan 20, profile 30, ratio 1, 1 arm, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 20, profile 30, ratio 1, 2 arms opposite, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 20, profile 30, ratio 1, 2 arms opposite, bend 90, same section"), // FoldBack 1, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // FoldBack 1, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 1, 2 arms same side, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 20, profile 30, ratio 1, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 1, 2 arms same side, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 1, 3 arms, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 20, profile 30, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 2, SliverTriangle 4, FoldBack 7
    QStringLiteral("plan 20, profile 30, ratio 2.5, 1 arm, bend 0, same section"), // Crease 13, SelfIntersection 16, FoldBack 9, WallMissed 2
    QStringLiteral("plan 20, profile 30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 13, SelfIntersection 23, FoldBack 9, WallMissed 2
    QStringLiteral("plan 20, profile 30, ratio 2.5, 1 arm, bend 40, same section"), // Crease 7, SelfIntersection 13, FoldBack 9
    QStringLiteral("plan 20, profile 30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 5, SelfIntersection 29
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 7, SelfIntersection 92, FoldBack 18, CapExposed 2
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 7, SelfIntersection 71, FoldBack 18, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 8, SelfIntersection 39, FoldBack 18, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 3, SelfIntersection 33, FoldBack 9, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 3, SelfIntersection 20, FoldBack 9, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 3, SelfIntersection 18, FoldBack 9, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 14, DegenerateTriangle 2, SelfIntersection 58, FoldBack 13, CapExposed 2
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 13, SelfIntersection 23, FoldBack 13, CapExposed 2
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 4, SelfIntersection 14, FoldBack 13, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 5, SelfIntersection 28, FoldBack 4, WallMissed 1, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms same side, bend 90, same section"), // FoldBack 4, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // FoldBack 4, CapExposed 1
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 0, same section"), // Crease 17, DegenerateTriangle 2, SliverTriangle 3, SelfIntersection 77, FoldBack 17
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 11, SliverTriangle 2, SelfIntersection 37, FoldBack 15
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 40, same section"), // Crease 7, DegenerateTriangle 2, SliverTriangle 1, SelfIntersection 50, FoldBack 17
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 7, SliverTriangle 1, SelfIntersection 9, FoldBack 6
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 90, same section"), // Crease 2, SelfIntersection 9
    QStringLiteral("plan 20, profile 30, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 2, SliverTriangle 4, SelfIntersection 32, FoldBack 10
    QStringLiteral("plan 20, profile 60, ratio 0.3, 2 arms same side, bend 0, same section"), // SliverTriangle 1
    QStringLiteral("plan 20, profile 60, ratio 0.3, 2 arms same side, bend 0, section x2 past J"), // SliverTriangle 1
    QStringLiteral("plan 20, profile 60, ratio 1, 1 arm, bend 0, section x2 past J"), // CenterlineOutside 1
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms opposite, bend 0, same section"), // Crease 1, FoldBack 2
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 1, FoldBack 8, CapExposed 1
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 1, FoldBack 8, CapExposed 1
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms same side, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms same side, bend 40, same section"), // Crease 1
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms same side, bend 40, section x2 past J"), // Crease 1
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 1, FoldBack 4, CapExposed 1
    QStringLiteral("plan 20, profile 60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 1, FoldBack 4, CapExposed 1
    QStringLiteral("plan 20, profile 60, ratio 1, 3 arms, bend 0, section x2 past J"), // CenterlineOutside 1
    QStringLiteral("plan 20, profile 60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 3, FoldBack 9
    QStringLiteral("plan 20, profile 60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 30, SelfIntersection 161, FoldBack 10, WallMissed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 26, SelfIntersection 145, FoldBack 10, WallMissed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 10, SliverTriangle 1, SelfIntersection 35, FoldBack 10
    QStringLiteral("plan 20, profile 60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 13, SelfIntersection 56
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 29, SelfIntersection 98, FoldBack 20, WallMissed 4, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 17, SelfIntersection 96, FoldBack 20, WallMissed 4, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 22, SliverTriangle 3, SelfIntersection 62, FoldBack 20, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 19, SliverTriangle 2, SelfIntersection 98, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 14, SliverTriangle 1, SelfIntersection 38, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 13, SliverTriangle 1, SelfIntersection 15, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 33, SliverTriangle 1, SelfIntersection 211, FoldBack 18, WallMissed 1, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 33, SelfIntersection 170, FoldBack 18, WallMissed 1, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 22, SliverTriangle 1, SelfIntersection 40, FoldBack 18, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 16, SelfIntersection 62, FoldBack 16, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 7, SelfIntersection 7, FoldBack 16, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 9, SelfIntersection 7, FoldBack 16, WallMissed 1, CapExposed 2
    QStringLiteral("plan 20, profile 60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 40, SelfIntersection 203, FoldBack 19, WallMissed 4
    QStringLiteral("plan 20, profile 60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 29, SelfIntersection 161, FoldBack 18, WallMissed 4
    QStringLiteral("plan 20, profile 60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 28, SliverTriangle 4, SelfIntersection 79, FoldBack 19
    QStringLiteral("plan 20, profile 60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 21, SliverTriangle 1, SelfIntersection 68, FoldBack 8
    QStringLiteral("plan 20, profile 60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 1
    QStringLiteral("plan 20, profile 60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 6, SelfIntersection 18, FoldBack 10
    QStringLiteral("plan 20, profile -30, ratio 1, 1 arm, bend 0, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 20, profile -30, ratio 1, 2 arms opposite, bend 0, same section"), // FoldBack 1
    QStringLiteral("plan 20, profile -30, ratio 1, 2 arms opposite, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 1, 2 arms same side, bend 0, section x2 past J"), // WallMissed 1
    QStringLiteral("plan 20, profile -30, ratio 1, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 1, 2 arms same side, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 1, 3 arms, bend 90, section x2 past J"), // SliverTriangle 4, FoldBack 7
    QStringLiteral("plan 20, profile -30, ratio 2.5, 1 arm, bend 0, same section"), // Crease 17, SelfIntersection 22, FoldBack 9, WallMissed 2
    QStringLiteral("plan 20, profile -30, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 16, SelfIntersection 17, FoldBack 9, WallMissed 2
    QStringLiteral("plan 20, profile -30, ratio 2.5, 1 arm, bend 40, same section"), // Crease 12, DegenerateTriangle 2, SelfIntersection 106, FoldBack 9
    QStringLiteral("plan 20, profile -30, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 2, SelfIntersection 23
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 2, SelfIntersection 1, CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // WallMissed 2, CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms opposite, bend 40, same section"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms opposite, bend 90, same section"), // FoldBack 2, CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 16, SelfIntersection 25, FoldBack 9, CapExposed 2
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 16, SelfIntersection 24, FoldBack 9, CapExposed 2
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 9, DegenerateTriangle 2, SelfIntersection 104, FoldBack 9, WallMissed 1, CapExposed 2
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 2, SelfIntersection 22, CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms same side, bend 90, same section"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // CapExposed 1
    QStringLiteral("plan 20, profile -30, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 1, SliverTriangle 1, SelfIntersection 27, FoldBack 10, CapExposed 1
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms opposite, bend 0, same section"), // Crease 1, FoldBack 4
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms opposite, bend 0, section x2 past J"), // Crease 1
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms opposite, bend 40, same section"), // Crease 1
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms opposite, bend 40, section x2 past J"), // Crease 1, FoldBack 3
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms opposite, bend 90, same section"), // Crease 2, FoldBack 6, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms opposite, bend 90, section x2 past J"), // Crease 2, FoldBack 6, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms same side, bend 0, same section"), // Crease 1
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms same side, bend 90, same section"), // Crease 3, FoldBack 3, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 1, 2 arms same side, bend 90, section x2 past J"), // Crease 4, FoldBack 3, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 1, 3 arms, bend 90, section x2 past J"), // Crease 3, SliverTriangle 1, FoldBack 9
    QStringLiteral("plan 20, profile -60, ratio 2.5, 1 arm, bend 0, same section"), // Crease 27, SliverTriangle 2, SelfIntersection 102, FoldBack 10, WallMissed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 1 arm, bend 0, section x2 past J"), // Crease 26, SliverTriangle 2, SelfIntersection 94, FoldBack 10, WallMissed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 1 arm, bend 40, same section"), // Crease 15, SelfIntersection 101, FoldBack 10
    QStringLiteral("plan 20, profile -60, ratio 2.5, 1 arm, bend 40, section x2 past J"), // Crease 14, SelfIntersection 40
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms opposite, bend 0, same section"), // Crease 24, SelfIntersection 73, FoldBack 20, WallMissed 4, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms opposite, bend 0, section x2 past J"), // Crease 17, DegenerateTriangle 2, SelfIntersection 73, FoldBack 20, WallMissed 4, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms opposite, bend 40, same section"), // Crease 16, SelfIntersection 59, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms opposite, bend 40, section x2 past J"), // Crease 17, SelfIntersection 61, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms opposite, bend 90, same section"), // Crease 13, SliverTriangle 1, SelfIntersection 53, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms opposite, bend 90, section x2 past J"), // Crease 13, SelfIntersection 40, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms same side, bend 0, same section"), // Crease 29, SliverTriangle 2, SelfIntersection 128, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms same side, bend 0, section x2 past J"), // Crease 35, SliverTriangle 2, SelfIntersection 121, FoldBack 17, WallMissed 1, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms same side, bend 40, same section"), // Crease 23, SelfIntersection 102, FoldBack 17, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms same side, bend 40, section x2 past J"), // Crease 19, SelfIntersection 47, FoldBack 14, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms same side, bend 90, same section"), // Crease 7, SelfIntersection 7, FoldBack 14, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 2 arms same side, bend 90, section x2 past J"), // Crease 9, SliverTriangle 3, SelfIntersection 7, FoldBack 14, WallMissed 1, CapExposed 2
    QStringLiteral("plan 20, profile -60, ratio 2.5, 3 arms, bend 0, same section"), // Crease 10, SelfIntersection 182, FoldBack 19, WallMissed 4
    QStringLiteral("plan 20, profile -60, ratio 2.5, 3 arms, bend 0, section x2 past J"), // Crease 13, DegenerateTriangle 4, SelfIntersection 95, FoldBack 18, WallMissed 4
    QStringLiteral("plan 20, profile -60, ratio 2.5, 3 arms, bend 40, same section"), // Crease 15, SliverTriangle 1, SelfIntersection 132, FoldBack 9
    QStringLiteral("plan 20, profile -60, ratio 2.5, 3 arms, bend 40, section x2 past J"), // Crease 15, SliverTriangle 1, SelfIntersection 94, FoldBack 8
    QStringLiteral("plan 20, profile -60, ratio 2.5, 3 arms, bend 90, same section"), // Crease 3, SelfIntersection 58
    QStringLiteral("plan 20, profile -60, ratio 2.5, 3 arms, bend 90, section x2 past J"), // Crease 8, SelfIntersection 78, FoldBack 10
    QStringLiteral("random seed 1, 1 junctions"), // FoldBack 11
    QStringLiteral("random seed 4, 4 junctions"), // Crease 1, SelfIntersection 28, FoldBack 11
    QStringLiteral("random seed 5, 5 junctions"), // FoldBack 4
    QStringLiteral("random seed 6, 6 junctions"), // FoldBack 6, WallMissed 3
    QStringLiteral("random seed 7, 1 junctions"), // Crease 1
    QStringLiteral("random seed 8, 2 junctions"), // FoldBack 2
    QStringLiteral("random seed 9, 3 junctions"), // Crease 1
    QStringLiteral("random seed 11, 5 junctions"), // SliverTriangle 3, FoldBack 15, WallMissed 1
    QStringLiteral("random seed 12, 6 junctions"), // FoldBack 3
    QStringLiteral("random seed 13, 1 junctions"), // FoldBack 8
    QStringLiteral("random seed 15, 3 junctions"), // FoldBack 4
    QStringLiteral("random seed 16, 4 junctions"), // Crease 12, FoldBack 7
    QStringLiteral("random seed 17, 5 junctions"), // Crease 2, WallMissed 2
    QStringLiteral("random seed 18, 6 junctions"), // Crease 7, WallMissed 2
    QStringLiteral("random seed 20, 2 junctions"), // FoldBack 12
    QStringLiteral("random seed 22, 4 junctions"), // FoldBack 2
    QStringLiteral("random seed 24, 6 junctions"), // WallMissed 1
    QStringLiteral("random seed 27, 3 junctions"), // FoldBack 9
    QStringLiteral("random seed 28, 4 junctions"), // Crease 21, FoldBack 1
    QStringLiteral("random seed 29, 5 junctions"), // Crease 6, SliverTriangle 11, SelfIntersection 119, FoldBack 21, WallBeyondReading 1, WallMissed 1
    QStringLiteral("random seed 30, 6 junctions"), // Crease 2, SliverTriangle 3, SelfIntersection 35, FoldBack 14, WallMissed 1
    QStringLiteral("random seed 32, 2 junctions"), // Crease 6, SelfIntersection 27, FoldBack 26
    QStringLiteral("random seed 32, 0 junctions"), // Crease 6, SelfIntersection 27, FoldBack 13
    QStringLiteral("random seed 33, 3 junctions"), // Crease 4, SliverTriangle 1, SelfIntersection 3, WallBeyondReading 1, FoldBack 7, WallMissed 1, CapExposed 2
    QStringLiteral("random seed 34, 4 junctions"), // Crease 1, FoldBack 7, WallMissed 1
    QStringLiteral("random seed 35, 5 junctions"), // Crease 5, SelfIntersection 1, FoldBack 9, RadiusJump 1, WallBeyondReading 2, WallMissed 1, CapExposed 5
    QStringLiteral("random seed 39, 3 junctions"), // FoldBack 4
    QStringLiteral("random seed 40, 4 junctions"), // Crease 1, SelfIntersection 5, FoldBack 6, WallMissed 1
    QStringLiteral("random seed 41, 5 junctions"), // FoldBack 2, WallMissed 2
    QStringLiteral("random seed 42, 6 junctions"), // Crease 5, FoldBack 10, WallMissed 2
    QStringLiteral("random seed 44, 2 junctions"), // Crease 11
    QStringLiteral("random seed 45, 3 junctions"), // FoldBack 7
    QStringLiteral("random seed 46, 4 junctions"), // Crease 4
    QStringLiteral("random seed 47, 5 junctions"), // FoldBack 12, WallMissed 1
    QStringLiteral("random seed 48, 6 junctions"), // Crease 1, FoldBack 4, WallMissed 3
    QStringLiteral("wild seed 8, 2 junctions"), // DegenerateTriangle
    QStringLiteral("wild seed 46, 4 junctions"), // DegenerateTriangle
};

cwPassageTubeBuilder::Debug buildDebug(const Cases::Survey& survey, double sampleSpacing)
{
    return cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, builderOptions(sampleSpacing));
}

std::pair<unsigned int, int> randomSurvey(int seed)
{
    return {static_cast<unsigned int>(seed), 1 + (seed - 1) % kMaxRandomJunctions};
}

QString randomSurveyName(unsigned int seed, int junctions)
{
    return QStringLiteral("random seed %1, %2 junctions").arg(seed).arg(junctions);
}

QString wildSurveyName(unsigned int seed, int junctions)
{
    return QStringLiteral("wild seed %1, %2 junctions").arg(seed).arg(junctions);
}

bool isOpenRandomSurvey(int seed, int junctions)
{
    return kUnionOpenCases.contains(randomSurveyName(static_cast<unsigned int>(seed), junctions));
}

// Every random survey, as (seed, junctions), with and without its junctions,
// that kUnionOpenCases names.
std::vector<std::pair<unsigned int, int>> openRandomSurveys()
{
    std::vector<std::pair<unsigned int, int>> surveys;
    for (int candidate = 1; candidate <= kRandomSeedCount; ++candidate) {
        const auto [seed, junctions] = randomSurvey(candidate);
        for (const int junctionCount : {junctions, 0}) {
            if (isOpenRandomSurvey(candidate, junctionCount)) {
                surveys.emplace_back(seed, junctionCount);
            }
        }
    }
    return surveys;
}

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
        return !isSweep(entry) && hasJunction(entry) && entry.name != kPhakeCave && !kUnionOpenCases.contains(entry.name);
    })));
    checkCaseBuildsClean(name);
}

// The open cases, with the sweep's subsampled, and every wild random survey.
TEST_CASE("cwPassageTubeCases open union cases build clean meshes", "[PassageTube][cwPassageTubeCases][!mayfail]")
{
    SECTION("catalog cases")
    {
        const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
            return !isSweep(entry) && kUnionOpenCases.contains(entry.name);
        })));
        checkCaseBuildsClean(name);
    }
    SECTION("sweep cases")
    {
        const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
            return isSweep(entry) && kUnionOpenCases.contains(entry.name);
        }, kSmokeStride)));
        checkCaseBuildsClean(name);
    }
    SECTION("random surveys")
    {
        const auto [seed, junctions] = GENERATE(from_range(openRandomSurveys()));
        INFO("seed " << seed << ", " << junctions << " junctions");
        checkBuildsClean(Cases::random(seed, junctions));
    }
    // Uniform bearings and independent readings make hairpin cones (Crease,
    // SliverTriangle, SelfIntersection), radius jumps between samples
    // (RadiusJump) and walls off a ring that a station's facing tilts
    // (WallMissed), so every wild survey stays open.
    SECTION("wild random surveys")
    {
        const auto [seed, junctions] = randomSurvey(GENERATE(range(1, kRandomSeedCount + 1)));
        const int junctionCount = GENERATE_COPY(0, junctions);
        INFO("seed " << seed << ", " << junctionCount << " junctions");
        checkBuildsClean(Cases::randomWild(seed, junctionCount));
    }
}

// Arms bigger than the passage they join.
TEST_CASE("cwPassageTubeCases big arms build clean through the union", "[PassageTube][cwPassageTubeCases]")
{
    const std::string name = GENERATE(as<std::string>(),
                                      "3. Big arm into a small passage",
                                      "16. Arm with its own section at J larger than the through",
                                      "18. Three big arms into a small passage");
    checkCaseBuildsClean(name);
}

// The union keeps an arm's bends, and the stations at them, inside the tube
// where the arm runs along a big through.
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

// Every issue Phake Cave keeps lies by a station kPhakeCaveIssues names.
TEST_CASE("cwPassageTubeCases Phake Cave keeps only its accepted issues", "[PassageTube][cwPassageTubeCases]")
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

// Arms as big as the through whose openings meet around it.
TEST_CASE("cwPassageTubeCases closes the union where big arms meet around the through", "[PassageTube][cwPassageTubeCases]")
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
        return isSweep(entry) && hasJunction(entry) && !kUnionOpenCases.contains(entry.name);
    }, kCleanSmokeStride)));
    checkCaseBuildsClean(name);
}

// The full sweep, run with "[Sweep]". It lacks the [PassageTube] tag because
// a tag filter also selects hidden tests, and the sweep would add minutes to
// every "[PassageTube]" run.
TEST_CASE("cwPassageTubeCases full sweep builds clean meshes", "[cwPassageTubeCases][Sweep][.]")
{
    const std::string name = GENERATE(from_range(caseNames([](const Cases::Case& entry) {
        return isSweep(entry) && hasJunction(entry) && !kUnionOpenCases.contains(entry.name);
    })));
    checkCaseBuildsClean(name);
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
        return !hasJunction(entry) && !kUnionOpenCases.contains(entry.name);
    })));
    checkCaseBuildsClean(name);
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
        return !isOpenRandomSurvey(candidate, randomSurvey(candidate).second);
    }, range(1, kRandomSeedCount + 1))));
    INFO("seed " << seed << ", " << junctions << " junctions");
    checkBuildsClean(Cases::random(seed, junctions));
}

// Seed 9's passage from s5 folds across itself; the union takes its tube as
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
    CHECK(crossing->station == QStringLiteral("s5"));
    CHECK(crossing->message.contains(QStringLiteral("station s5 crosses itself at ")));

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
        return !isOpenRandomSurvey(candidate, 0);
    }, range(1, kRandomSeedCount + 1)));
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
                          >= Cases::kRandomClearanceMeters - kClearanceToleranceMeters);
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
    if (kUnionOpenCases.contains(wildSurveyName(seed, junctionCount))) {
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
        CHECK(caseNames([](const Cases::Case& entry) { return entry.tags.contains(QStringLiteral("bend")); }).size() == 13);
        CHECK(caseNames([](const Cases::Case& entry) { return entry.tags.contains(QStringLiteral("miter")); }).size() == 5);
        CHECK(caseNames([](const Cases::Case& entry) { return entry.tags.contains(QStringLiteral("pitch")); }).size() == 4);
    }
}

// A junction is the union of its chains' tubes: a junction case reports only
// the warnings the union pipeline keeps, closes every chain end with a fan
// or leaves it open, and takes every triangle from a chain's tube.
TEST_CASE("cwPassageTubeBuilder has no junction stage", "[PassageTube][cwPassageTubeBuilder]")
{
    using Warning = cwPassageTubeMesh::Warning;
    using EndCap = cwPassageTubeBuilder::EndCap;
    static const QSet<Warning::Type> kUnionWarnings = {
        Warning::NoPassageData, Warning::ChainTorn, Warning::LrudOutOfRange, Warning::ScrapOpening,
        Warning::SourceChange, Warning::RepeatedShot, Warning::JunctionReadingConflict,
        Warning::ChainSelfCrossing, Warning::EnclosedVoid, Warning::TubeRejected};

    const std::string name = GENERATE(from_range(caseNames(hasJunction)));
    INFO(name);
    const Cases::Case& entry = catalogCase(name);
    const cwPassageTubeBuilder::Debug debug = buildDebug(Cases::toSurvey(entry), entry.sampleSpacing);

    for (const Warning& warning : debug.warnings) {
        INFO("warning " << static_cast<int>(warning.type) << " at " << warning.station.toStdString()
                        << ": " << warning.message.toStdString());
        CHECK(kUnionWarnings.contains(warning.type));
    }

    for (int i = 0; i < debug.chains.size(); ++i) {
        const cwPassageTubeBuilder::Chain& chain = debug.chains.at(i);
        INFO("chain " << i);
        CHECK((chain.startCap == EndCap::Fan || chain.startCap == EndCap::None));
        CHECK((chain.endCap == EndCap::Fan || chain.endCap == EndCap::None));
    }

    const int triangleCount = static_cast<int>(debug.mesh.indices.size() / 3);
    CHECK(debug.triangleChain.size() == triangleCount);
    const int outsideChainCount = static_cast<int>(std::count_if(debug.triangleChain.cbegin(), debug.triangleChain.cend(), [&debug](int chain) {
        return chain < 0 || chain >= debug.chains.size();
    }));
    INFO("triangles " << triangleCount << ", chains " << debug.chains.size());
    CHECK(outsideChainCount == 0);
}
