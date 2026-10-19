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
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeCases.h"
#include "cwPassageTubeMeshCheck.h"
#include "cwPassageTubeUnion.h"

//Qt includes
#include <QElapsedTimer>
#include <QtMath>
#include <QDebug>

//Std includes
#include <algorithm>
#include <cmath>

namespace {

using Builder = cwPassageTubeBuilder;
using Cases = cwPassageTubeCases;
using Check = cwPassageTubeMeshCheck;
using Union = cwPassageTubeUnion;

constexpr double kVolumeTolerance = 1e-4;


constexpr int kRandomSeed = 1;
constexpr int kRandomJunctions = 300;

// Station a6 of the slot dropping into the plumb shaft to a9, and its floor
// passage, in meters (x east, y north, z up).
const QVector3D kA6(29.45f, 5.85f, -6.5f);
const QVector3D kA9(29.45f, 5.85f, -29.94f);
constexpr double kSlotLegMeters = 16.5;
constexpr double kSlotDropMeters = 2.2;
constexpr double kSlotFacingDegrees = 147.0;
constexpr double kShaftFacingDegrees = 230.0;
constexpr double kFloorLegMeters = 5.8;

struct BoxSection {
    double left = 0.0;
    double right = 0.0;
    double up = 0.0;
    double down = 0.0;
};

const BoxSection kSlotSection {5.0, 0.05, 3.0, 23.0};
const BoxSection kShaftTopSection {1.0, 5.0, 2.0, 1.2};
const BoxSection kShaftBottomSection {2.3, 2.0, 24.0, 0.05};
// a10 has no readings in the source; its floor passage closes at an
// ordinary 2 m ceiling.
const BoxSection kFloorEndSection {2.3, 2.0, 2.0, 0.05};

QVector3D planDirection(double bearingDegrees)
{
    const double bearing = qDegreesToRadians(bearingDegrees);
    return QVector3D(static_cast<float>(std::sin(bearing)), static_cast<float>(std::cos(bearing)), 0.0f);
}

// A vertical rectangle around station: left and right level along the
// facing turned clockwise, up and down plumb.
QVector<QVector3D> boxRing(const QVector3D& station, double facingDegrees, const BoxSection& section)
{
    const QVector3D facing = planDirection(facingDegrees);
    const QVector3D right(facing.y(), -facing.x(), 0.0f);
    const QVector3D up(0.0f, 0.0f, 1.0f);
    const auto corner = [&](double side, double height) {
        return station + right * static_cast<float>(side) + up * static_cast<float>(height);
    };
    return {corner(section.right, section.up),
            corner(-section.left, section.up),
            corner(-section.left, -section.down),
            corner(section.right, -section.down)};
}

cwPassageTubeMesh axisBox(const QVector3D& lo, const QVector3D& hi)
{
    const auto ring = [&](float x) {
        return QVector<QVector3D> {{x, lo.y(), lo.z()}, {x, hi.y(), lo.z()}, {x, hi.y(), hi.z()}, {x, lo.y(), hi.z()}};
    };
    return Union::loft({ring(lo.x()), ring(hi.x())});
}

// The tubes one after another, each triangle tagged with its tube as a chain
// so the checker reports their crossings as PassageOverlap.
struct Overlap {
    cwPassageTubeMesh mesh;
    Builder::Debug debug;
};

Overlap overlapOf(const QVector<cwPassageTubeMesh>& tubes)
{
    Overlap overlap;
    overlap.mesh = Union::merged(tubes);
    for (int t = 0; t < tubes.size(); ++t) {
        Builder::Leg leg;
        leg.from = QStringLiteral("tube%1").arg(t);
        overlap.debug.legs.append(leg);
        Builder::Chain chain;
        chain.legs = {Builder::ChainLeg {t, false}};
        overlap.debug.chains.append(chain);

        const int triangles = static_cast<int>(tubes.at(t).indices.size() / 3);
        for (int i = 0; i < triangles; ++i) {
            overlap.debug.triangleChain.append(t);
            overlap.debug.triangleInZipBand.append(false);
            overlap.debug.triangleCap.append(-1);
        }
    }
    return overlap;
}

QString issuesText(const QVector<Check::Issue>& issues)
{
    Check::Report report;
    report.issues = issues;
    return Check::toText(report);
}

int countOf(const QVector<Check::Issue>& issues, Check::Type type)
{
    return static_cast<int>(std::count_if(issues.cbegin(), issues.cend(), [&](const Check::Issue& issue) {
        return issue.type == type;
    }));
}

// Every input is a closed manifold with the checker's blessing, the inputs
// cross each other, and their union is one clean closed surface.
Union::Result checkUnion(const QVector<cwPassageTubeMesh>& tubes, const QString& label)
{
    for (int t = 0; t < tubes.size(); ++t) {
        INFO(label.toStdString() << " input " << t);
        const Check::Report closed = Check::closed(tubes.at(t), Check::wholeMesh(tubes.at(t)));
        INFO(Check::toText(closed).toStdString());
        CHECK(closed.issues.isEmpty());
        CHECK(closed.componentCount == 1);
    }

    const Overlap overlap = overlapOf(tubes);
    const QVector<Check::Issue> inputCrossings = Check::selfIntersections(overlap.mesh, Check::wholeMesh(overlap.mesh), overlap.debug);
    CHECK(countOf(inputCrossings, Check::PassageOverlap) > 0);

    QElapsedTimer timer;
    timer.start();
    const Union::Result result = Union::unite(tubes);
    const qint64 elapsed = timer.nsecsElapsed();
    qInfo().noquote() << QStringLiteral("%1: union of %2 tubes in %3 ms, %4 triangles, volume %5 m3, genus %6")
                             .arg(label)
                             .arg(tubes.size())
                             .arg(static_cast<double>(elapsed) / 1.0e6, 0, 'f', 2)
                             .arg(result.mesh.indices.size() / 3)
                             .arg(result.volume, 0, 'f', 2)
                             .arg(result.genus);

    for (int t = 0; t < result.inputStatus.size(); ++t) {
        INFO(label.toStdString() << " input " << t << " " << Union::statusName(result.inputStatus.at(t)).toStdString());
        CHECK(result.inputStatus.at(t) == Union::Status::NoError);
    }
    INFO(Union::statusName(result.status).toStdString());
    REQUIRE(result.status == Union::Status::NoError);
    CHECK(result.genus == 0);

    const Check::Range range = Check::wholeMesh(result.mesh);
    const Check::Report closed = Check::closed(result.mesh, range);
    {
        INFO(Check::toText(closed).toStdString());
        CHECK(closed.issues.isEmpty());
        CHECK(closed.componentCount == 1);
    }
    const QVector<Check::Issue> degenerate = Check::degenerateTriangles(result.mesh, range);
    {
        INFO(issuesText(degenerate).toStdString());
        CHECK(degenerate.isEmpty());
    }
    const QVector<Check::Issue> crossings = Check::selfIntersections(result.mesh, range);
    {
        INFO(issuesText(crossings).toStdString());
        CHECK(crossings.isEmpty());
    }
    return result;
}

Builder::Debug chainRings(const Cases::Survey& survey)
{
    return Builder::buildChainRings(survey.region, survey.positionsByCave, Builder::Options());
}

QVector<cwPassageTubeMesh> meshesOf(const QVector<Union::ChainTube>& tubes)
{
    QVector<cwPassageTubeMesh> meshes;
    for (const Union::ChainTube& tube : tubes) {
        meshes.append(tube.mesh);
    }
    return meshes;
}

Cases::JunctionCase plainT()
{
    const QVector<Cases::JunctionCase> cases = Cases::galleryCases();
    const auto it = std::find_if(cases.cbegin(), cases.cend(), [](const Cases::JunctionCase& entry) {
        return entry.name.startsWith(QStringLiteral("1. Symmetric T"));
    });
    REQUIRE(it != cases.cend());
    return *it;
}

QVector<cwPassageTubeMesh> phakeA6Boxes()
{
    const QVector3D slotDirection = planDirection(kSlotFacingDegrees);
    const double slotRun = std::sqrt(kSlotLegMeters * kSlotLegMeters - kSlotDropMeters * kSlotDropMeters);
    const QVector3D a4 = kA6 - slotDirection * static_cast<float>(slotRun) + QVector3D(0.0f, 0.0f, static_cast<float>(kSlotDropMeters));
    const cwPassageTubeMesh slot = Union::loft({boxRing(a4, kSlotFacingDegrees, kSlotSection),
                                                boxRing(kA6, kSlotFacingDegrees, kSlotSection)});

    // Redesign step 5: a plumb leg's rings stand pad = mean(L, R) / 2 out
    // from each station along the chain's plan direction.
    const auto pad = [](const BoxSection& section) { return (section.left + section.right) / 4.0; };
    const QVector3D shaftDirection = planDirection(kShaftFacingDegrees);
    const cwPassageTubeMesh shaft = Union::loft(
        {boxRing(kA6 - shaftDirection * static_cast<float>(pad(kShaftTopSection)), kShaftFacingDegrees, kShaftTopSection),
         boxRing(kA9 + shaftDirection * static_cast<float>(pad(kShaftBottomSection)), kShaftFacingDegrees, kShaftBottomSection)});

    const QVector3D a10 = kA9 + shaftDirection * static_cast<float>(kFloorLegMeters);
    const cwPassageTubeMesh floor = Union::loft({boxRing(kA9, kShaftFacingDegrees, kShaftBottomSection),
                                                 boxRing(a10, kShaftFacingDegrees, kFloorEndSection)});
    return {slot, shaft, floor};
}

} // namespace

TEST_CASE("Two overlapping boxes unite into one closed solid", "[PassageTubeUnion]")
{
    const cwPassageTubeMesh first = axisBox({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f});
    const cwPassageTubeMesh second = axisBox({1.0f, 1.0f, 0.0f}, {3.0f, 3.0f, 3.0f});
    constexpr double kFirstVolume = 8.0;
    constexpr double kSecondVolume = 12.0;
    constexpr double kSharedVolume = 2.0;

    CHECK(Union::status(first) == Union::Status::NoError);
    CHECK(Union::volume(first) == Catch::Approx(kFirstVolume).epsilon(kVolumeTolerance));
    CHECK(Union::volume(second) == Catch::Approx(kSecondVolume).epsilon(kVolumeTolerance));

    const Union::Result result = checkUnion({first, second}, QStringLiteral("boxes"));
    CHECK(result.isComplete());
    CHECK(result.volume == Catch::Approx(kFirstVolume + kSecondVolume - kSharedVolume).epsilon(kVolumeTolerance));
}

TEST_CASE("An open tube is named and left out of the union", "[PassageTubeUnion]")
{
    cwPassageTubeMesh open = axisBox({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    open.indices.resize(open.indices.size() - 3);
    const cwPassageTubeMesh closed = axisBox({2.0f, 0.0f, 0.0f}, {3.0f, 1.0f, 1.0f});

    const Union::Result result = Union::unite({open, closed});
    REQUIRE(result.inputStatus.size() == 2);
    CHECK(result.inputStatus.at(0) == Union::Status::NotManifold);
    CHECK(result.inputStatus.at(1) == Union::Status::NoError);
    CHECK_FALSE(result.isComplete());
    CHECK(result.volume == Catch::Approx(1.0).epsilon(kVolumeTolerance));
}

TEST_CASE("The T junction's chain tubes unite into one clean surface", "[PassageTubeUnion]")
{
    const Builder::Debug rings = chainRings(Cases::toSurvey(plainT()));
    const QVector<Union::ChainTube> tubes = Union::chainTubes(rings);
    REQUIRE(tubes.size() == 2);
    CHECK(Union::chainComponents(rings).size() == 1);

    const Union::Result result = checkUnion(meshesOf(tubes), QStringLiteral("T junction"));
    CHECK(result.volume < Union::volume(tubes.at(0).mesh) + Union::volume(tubes.at(1).mesh));

    // The arm's cap at J is square to it and inside the through, apart from
    // vertices on the through's ceiling and floor, which count as outside.
    const QVector<Union::CapVertex> notch = Union::capNotch(rings);
    qInfo().noquote() << QStringLiteral("T junction: %1 cap vertices outside the other tube").arg(notch.size());
    for (const Union::CapVertex& vertex : notch) {
        qInfo().noquote() << QStringLiteral("  chain %1 %2 bin %3 at (%4, %5, %6)")
                                 .arg(vertex.chain)
                                 .arg(vertex.atStart ? QStringLiteral("start") : QStringLiteral("end"))
                                 .arg(vertex.bin)
                                 .arg(vertex.from.x(), 0, 'f', 2)
                                 .arg(vertex.from.y(), 0, 'f', 2)
                                 .arg(vertex.from.z(), 0, 'f', 2);
    }
}

TEST_CASE("The 30 degree fork's chain tubes unite, with its arm cap outside the through", "[PassageTubeUnion]")
{
    const Cases::JunctionCase fork = Cases::wideArmFork();
    const Builder::Debug rings = chainRings(Cases::toSurvey(fork));
    const QVector<Union::ChainTube> tubes = Union::chainTubes(rings);
    REQUIRE(tubes.size() == 2);
    checkUnion(meshesOf(tubes), QStringLiteral("fork"));

    const QVector<Union::CapVertex> notch = Union::capNotch(rings);
    qInfo().noquote() << QStringLiteral("fork: %1 cap vertices outside the other tube").arg(notch.size());
    for (const Union::CapVertex& vertex : notch) {
        qInfo().noquote() << QStringLiteral("  chain %1 %2 bin %3 at (%4, %5, %6)")
                                 .arg(vertex.chain)
                                 .arg(vertex.atStart ? QStringLiteral("start") : QStringLiteral("end"))
                                 .arg(vertex.bin)
                                 .arg(vertex.from.x(), 0, 'f', 2)
                                 .arg(vertex.from.y(), 0, 'f', 2)
                                 .arg(vertex.from.z(), 0, 'f', 2);
    }
    CHECK_FALSE(notch.isEmpty());
}

TEST_CASE("A slot, a plumb shaft and a floor passage unite as three boxes", "[PassageTubeUnion]")
{
    const QVector<cwPassageTubeMesh> boxes = phakeA6Boxes();
    for (const cwPassageTubeMesh& box : boxes) {
        CHECK(Union::status(box) == Union::Status::NoError);
    }
    checkUnion(boxes, QStringLiteral("a6 boxes"));
}

TEST_CASE("Union cost on a random cave", "[.PassageTubeUnionTiming]")
{
    const Builder::Debug rings = chainRings(Cases::toSurvey(Cases::random(kRandomSeed, kRandomJunctions)));
    const QVector<Union::ChainTube> tubes = Union::chainTubes(rings);
    const QVector<QVector<int>> components = Union::chainComponents(rings);

    QElapsedTimer total;
    total.start();
    int failedInputs = 0;
    for (int c = 0; c < components.size(); ++c) {
        QVector<cwPassageTubeMesh> meshes;
        int triangles = 0;
        for (const Union::ChainTube& tube : tubes) {
            if (components.at(c).contains(tube.chain)) {
                meshes.append(tube.mesh);
                triangles += static_cast<int>(tube.mesh.indices.size() / 3);
            }
        }
        int selfCrossingTubes = 0;
        int degenerateTubes = 0;
        for (const cwPassageTubeMesh& mesh : std::as_const(meshes)) {
            selfCrossingTubes += Check::selfIntersections(mesh, Check::wholeMesh(mesh)).isEmpty() ? 0 : 1;
            degenerateTubes += Check::degenerateTriangles(mesh, Check::wholeMesh(mesh)).isEmpty() ? 0 : 1;
        }
        qInfo().noquote() << QStringLiteral("  inputs: %1 tubes cross themselves, %2 have degenerate or sliver triangles").arg(selfCrossingTubes).arg(degenerateTubes);

        QElapsedTimer timer;
        timer.start();
        const Union::Result result = Union::unite(meshes);
        const qint64 elapsed = timer.nsecsElapsed();
        failedInputs += static_cast<int>(std::count_if(result.inputStatus.cbegin(), result.inputStatus.cend(), [](Union::Status status) {
            return status != Union::Status::NoError;
        }));
        qInfo().noquote() << QStringLiteral("random(%1, %2) component %3: %4 tubes, %5 input triangles, union in %6 ms, %7 triangles, status %8")
                                 .arg(kRandomSeed)
                                 .arg(kRandomJunctions)
                                 .arg(c)
                                 .arg(meshes.size())
                                 .arg(triangles)
                                 .arg(static_cast<double>(elapsed) / 1.0e6, 0, 'f', 1)
                                 .arg(result.mesh.indices.size() / 3)
                                 .arg(Union::statusName(result.status));
        CHECK(result.status == Union::Status::NoError);

        const Check::Report closed = Check::closed(result.mesh, Check::wholeMesh(result.mesh));
        qInfo().noquote() << QStringLiteral("  closed: %1 issues, %2 components").arg(closed.issues.size()).arg(closed.componentCount);
        qInfo().noquote() << Check::toText(closed);
        for (const Check::ComponentStats& stats : closed.components) {
            qInfo().noquote() << QStringLiteral("  component %1 triangles, volume %2").arg(stats.triangleCount).arg(stats.volume);
        }
        const QVector<Check::Issue> degenerate = Check::degenerateTriangles(result.mesh, Check::wholeMesh(result.mesh));
        qInfo().noquote() << QStringLiteral("  %1 degenerate or sliver triangles").arg(degenerate.size());
        const QVector<Check::Issue> crossings = Check::selfIntersections(result.mesh, Check::wholeMesh(result.mesh));
        qInfo().noquote() << QStringLiteral("  %1 self-intersections").arg(crossings.size());
    }
    qInfo().noquote() << QStringLiteral("random(%1, %2): %3 chains, %4 tubes, %5 components, %6 inputs rejected, total %7 ms")
                             .arg(kRandomSeed)
                             .arg(kRandomJunctions)
                             .arg(rings.chains.size())
                             .arg(tubes.size())
                             .arg(components.size())
                             .arg(failedInputs)
                             .arg(total.elapsed());
}
