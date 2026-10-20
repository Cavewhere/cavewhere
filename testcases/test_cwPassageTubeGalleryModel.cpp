/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeCases.h"
#include "cwPassageTubeGalleryModel.h"
#include "cwPassageTubeSandboxModel.h"
#include "cwPassageTubeUnion.h"
#include "cwProject.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPassageTube.h"
#include "cwScene.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Qt includes
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

//Std includes
#include <algorithm>
#include <cmath>

namespace {

using Model = cwPassageTubeGalleryModel;
using Sandbox = cwPassageTubeSandboxModel;

const QString kSymmetricT = QStringLiteral("1. symmetric T");
const QString kJunctionFreeCase = QStringLiteral("no arms, through profile 0, bend 0, same section");
const QString kShaftFootCase = QStringLiteral("25. Zero floor reading at the foot of a shaft");
const QString kSharpBendCase = QStringLiteral("23. Sharp bend at a station reading zero on the outside wall");
// c1 joins a plumb section to a level one, stitched until T07ah3.
const QString kCaseWithIssues = kSharpBendCase;
constexpr float kLengthTolerance = 1e-4f;
constexpr float kPositionTolerance = 1e-3f;
constexpr int kProjectLoadTimeoutMs = 30000;

const QString kProjectBaseName = QStringLiteral("Bend");
const QStringList kCaveNames {QStringLiteral("First Cave"), QStringLiteral("Second Cave")};
const QString kReading = QStringLiteral("1");

// a1 → a2 runs 10 m north, a2 → a3 turns east for 15 m, all level.
struct BendShot {
    const char* from;
    const char* to;
    const char* distance;
    const char* compass;
};
const BendShot kBendShots[] {{"a1", "a2", "10", "0"}, {"a2", "a3", "15", "90"}};
const QVector3D kA1(0.0f, 0.0f, 0.0f);
const QVector3D kA2(0.0f, 10.0f, 0.0f);
const QVector3D kA3(15.0f, 10.0f, 0.0f);

cwStation bendStation(const char* name)
{
    cwStation station(QString::fromLatin1(name));
    station.setLeft(cwDistanceReading(kReading));
    station.setRight(cwDistanceReading(kReading));
    station.setUp(cwDistanceReading(kReading));
    station.setDown(cwDistanceReading(kReading));
    return station;
}

// Saves a .cwproj with a cave per kCaveNames, each surveying kBendShots, and
// returns its path.
QString writeBendProject(const QDir& directory)
{
    cwProject project;
    cwCavingRegion* region = project.cavingRegion();
    for (const QString& caveName : kCaveNames) {
        auto* cave = new cwCave();
        cave->setName(caveName);
        region->addCave(cave);
        auto* trip = new cwTrip();
        trip->setName(QStringLiteral("Trip"));
        cave->addTrip(trip);
        auto* chunk = new cwSurveyChunk();
        trip->addChunk(chunk);
        for (const BendShot& bend : kBendShots) {
            cwShot shot;
            shot.setDistance(cwDistanceReading(QString::fromLatin1(bend.distance)));
            shot.setCompass(cwCompassReading(QString::fromLatin1(bend.compass)));
            shot.setClino(cwClinoReading(QStringLiteral("0")));
            chunk->appendShot(bendStation(bend.from), bendStation(bend.to), shot);
        }
    }

    const QString path = directory.filePath(kProjectBaseName + QStringLiteral("/") + kProjectBaseName + QStringLiteral(".cwproj"));
    REQUIRE(project.saveAs(path));
    project.waitSaveToFinish();
    REQUIRE(QFileInfo::exists(path));
    return path;
}

// The bend as a catalog trip, positioned as the line plot solves it.
cwPassageTubeCases::Trip bendTrip()
{
    const QStringList readings {kReading, kReading, kReading, kReading};
    cwPassageTubeCases::Trip trip;
    trip.chunks = {{{QStringLiteral("a1"), kA1, readings},
                    {QStringLiteral("a2"), kA2, readings},
                    {QStringLiteral("a3"), kA3, readings}}};
    return trip;
}

bool isNear(const QVector3D& a, const QVector3D& b)
{
    return a.distanceToPoint(b) < kPositionTolerance;
}

QString nameAt(const Model& model, int row)
{
    return model.data(model.index(row), Model::NameRole).toString();
}

// The rings of every chain of the named catalog case, built as the model
// builds it.
QVector<cwPassageTubeBuilder::Ring> caseRings(const QString& name)
{
    const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();
    const auto entry = std::find_if(catalog.cbegin(), catalog.cend(), [&](const cwPassageTubeCases::Case& candidate) {
        return candidate.name == name;
    });
    if (entry == catalog.cend()) {
        return {};
    }
    const cwPassageTubeCases::Survey survey = cwPassageTubeCases::toSurvey(*entry);
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = entry->sampleSpacing;
    const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, options);

    QVector<cwPassageTubeBuilder::Ring> rings;
    for (const cwPassageTubeBuilder::Chain& chain : debug.chains) {
        rings.append(chain.rings);
    }
    return rings;
}

// The position of station in the named catalog case's survey.
QVector3D casePosition(const QString& name, const QString& station)
{
    const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();
    const auto entry = std::find_if(catalog.cbegin(), catalog.cend(), [&](const cwPassageTubeCases::Case& candidate) {
        return candidate.name == name;
    });
    REQUIRE(entry != catalog.cend());
    const cwPassageTubeCases::Survey survey = cwPassageTubeCases::toSurvey(*entry);
    REQUIRE(survey.region.caves.size() == 1);
    const cwStationPositionLookup positions = survey.positionsByCave.value(survey.region.caves.constFirst().id);
    REQUIRE(positions.hasPosition(station));
    return positions.position(station);
}

} // namespace

TEST_CASE("cwPassageTubeGalleryModel lists the catalog", "[PassageTube][cwPassageTubeGalleryModel]")
{
    const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();

    Model model;
    CHECK(model.currentIndex() == -1);

    SECTION("without the sweep, the sandbox and the catalog's hand-made cases in order")
    {
        QStringList expected {Sandbox::sandboxName()};
        for (const cwPassageTubeCases::Case& entry : catalog) {
            if (!entry.tags.contains(QStringLiteral("sweep"))) {
                expected.append(entry.name);
            }
        }
        REQUIRE(model.rowCount() == expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            CHECK(nameAt(model, i) == expected.at(i));
        }
    }

    SECTION("with the sweep, the sandbox and every catalog case")
    {
        model.setIncludeSweep(true);
        REQUIRE(model.rowCount() == catalog.size() + 1);
        CHECK(nameAt(model, model.rowCount() - 1) == catalog.constLast().name);
    }
}

TEST_CASE("cwPassageTubeGalleryModel lists the sandbox first", "[PassageTube][cwPassageTubeGalleryModel]")
{
    const std::string sandboxName = "Sandbox";
    const QVector<cwPassageTubeCases::Case> catalog = cwPassageTubeCases::catalog();
    const auto handMadeCount = std::count_if(catalog.cbegin(), catalog.cend(), [](const cwPassageTubeCases::Case& entry) {
        return !entry.tags.contains(QStringLiteral("sweep"));
    });

    Model model;
    CHECK(model.rowCount() == handMadeCount + 1);
    CHECK(nameAt(model, 0).toStdString() == sandboxName);
    CHECK(model.indexOf(QString::fromStdString(sandboxName)) == 0);

    model.setCurrentIndex(0);
    CHECK(model.currentName().toStdString() == sandboxName);
    CHECK_FALSE(model.mesh().isEmpty());

    SECTION("and keeps it first with the sweep")
    {
        model.setIncludeSweep(true);
        CHECK(model.rowCount() == catalog.size() + 1);
        CHECK(nameAt(model, 0).toStdString() == sandboxName);
    }
}

TEST_CASE("cwPassageTubeGalleryModel loads a catalog case into the sandbox unchanged", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    const int row = model.indexOf(kSymmetricT);
    REQUIRE(row > Model::kSandboxRow);
    model.setCurrentIndex(row);
    const cwPassageTubeMesh catalogMesh = model.mesh();
    REQUIRE_FALSE(catalogMesh.isEmpty());

    CHECK_FALSE(model.canLoadSandbox(Model::kSandboxRow));
    CHECK_FALSE(model.canLoadSandbox(model.indexOf(QStringLiteral("Phake Cave"))));
    REQUIRE(model.canLoadSandbox(row));
    REQUIRE(model.loadSandbox(row));

    CHECK(model.currentIndex() == Model::kSandboxRow);
    CHECK(model.sandboxSelected());
    CHECK(model.sandbox()->junctionCase() == cwPassageTubeCases::galleryCases().constFirst());
    CHECK(model.mesh().positions == catalogMesh.positions);
    CHECK(model.mesh().indices == catalogMesh.indices);
    CHECK_FALSE(model.sandboxBuildText().isEmpty());
}

TEST_CASE("cwPassageTubeGalleryModel rebuilds the sandbox once per burst of edits", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    model.setCurrentIndex(Model::kSandboxRow);
    const cwPassageTubeMesh before = model.mesh();
    REQUIRE_FALSE(before.isEmpty());

    QSignalSpy currentSpy(&model, &Model::currentChanged);
    Sandbox* sandbox = model.sandbox();
    sandbox->addArm();
    sandbox->addArm();
    sandbox->setPlanDegrees(1, 30.0);
    sandbox->setWall(1, Sandbox::ArmFar, Sandbox::Left, 3.0);
    sandbox->setWall(1, Sandbox::ArmJunction, Sandbox::Left, 3.0);
    CHECK(currentSpy.isEmpty());

    REQUIRE(currentSpy.wait());
    CHECK(currentSpy.count() == 1);
    CHECK(model.currentIndex() == Model::kSandboxRow);
    CHECK(model.mesh().positions != before.positions);
    CHECK(model.parameters().contains(QStringLiteral("a10")));

    // Redesign step 3: the 30 degree arm's 3 m left wall leaves its end cap
    // notched outside the through, and sliding the caps moves it inside.
    using Union = cwPassageTubeUnion;
    const cwPassageTubeCases::Survey survey = cwPassageTubeCases::toSurvey(sandbox->junctionCase());
    cwPassageTubeBuilder::Options options;
    options.sampleSpacingMeters = sandbox->sampleSpacing();
    cwPassageTubeBuilder::Debug rings = cwPassageTubeBuilder::buildChainRings(survey.region, survey.positionsByCave, options);
    const qsizetype notched = Union::capNotch(rings).size();
    CHECK(notched > 0);
    const QVector<Union::CapVertex> slid = Union::slideCaps(rings);
    CHECK(std::any_of(slid.cbegin(), slid.cend(), [](const Union::CapVertex& vertex) { return vertex.isInside(); }));
    CHECK(Union::capNotch(rings).size() < notched);
}

TEST_CASE("cwPassageTubeGalleryModel coarsens the sandbox's sample spacing after a slow build", "[PassageTube][cwPassageTubeGalleryModel]")
{
    constexpr double kFineSpacingMeters = 0.05;
    constexpr double kLongArmMeters = 30.0;

    Model model;
    Sandbox* sandbox = model.sandbox();
    sandbox->addArm();
    sandbox->setLengthMeters(0, kLongArmMeters);
    sandbox->setSampleSpacing(kFineSpacingMeters);

    QElapsedTimer timer;
    timer.start();
    model.setCurrentIndex(Model::kSandboxRow);
    if (timer.elapsed() <= Model::kSandboxBuildBudgetMilliseconds) {
        SKIP("The fine sandbox built within the budget on this machine");
    }

    const QString slow = model.sandboxBuildText();
    INFO(slow.toStdString());
    CHECK(slow.contains(QStringLiteral("at 0.05 m sample spacing")));
    CHECK_FALSE(slow.contains(QStringLiteral("coarsened")));
    CHECK(slow.contains(QStringLiteral("next build uses 0.1 m")));

    model.setCurrentIndex(model.firstCaseRow());
    model.setCurrentIndex(Model::kSandboxRow);
    const QString coarse = model.sandboxBuildText();
    INFO(coarse.toStdString());
    CHECK(coarse.contains(QStringLiteral("at 0.1 m sample spacing, coarsened from 0.05 m")));

    sandbox->setSampleSpacing(1.0);
    QSignalSpy currentSpy(&model, &Model::currentChanged);
    REQUIRE(currentSpy.wait());
    const QString reset = model.sandboxBuildText();
    INFO(reset.toStdString());
    CHECK(reset.contains(QStringLiteral("at 1 m sample spacing")));
    CHECK_FALSE(reset.contains(QStringLiteral("coarsened")));
}

TEST_CASE("cwPassageTubeSandboxModel edits its junction case", "[PassageTube][cwPassageTubeSandboxModel]")
{
    Sandbox sandbox;
    CHECK(sandbox.rowCount() == 0);
    CHECK(sandbox.junctionCase().name == Sandbox::sandboxName());

    SECTION("an arm goes to the next free quarter turn")
    {
        QSignalSpy changedSpy(&sandbox, &Sandbox::caseChanged);
        sandbox.addArm();
        sandbox.addArm();
        CHECK(changedSpy.count() == 2);
        REQUIRE(sandbox.rowCount() == 2);
        CHECK(sandbox.junctionCase().arms.at(0).planDegrees == 90.0);
        CHECK(sandbox.junctionCase().arms.at(1).planDegrees == -90.0);
        CHECK(sandbox.selectedArm() == 1);

        sandbox.removeArm(sandbox.selectedArm());
        CHECK(sandbox.rowCount() == 1);
        CHECK(sandbox.selectedArm() == 0);
    }

    SECTION("a slider change produces a different survey")
    {
        sandbox.addArm();
        const cwPassageTubeCases::Trip before = cwPassageTubeCases::toTrip(sandbox.junctionCase());

        QSignalSpy changedSpy(&sandbox, &Sandbox::caseChanged);
        sandbox.setProfileDegrees(0, 45.0);
        CHECK(changedSpy.count() == 1);
        CHECK(cwPassageTubeCases::toTrip(sandbox.junctionCase()) != before);

        sandbox.setProfileDegrees(0, 45.0);
        CHECK(changedSpy.count() == 1);
    }

    SECTION("the arm's reading at J and the through's are separate records")
    {
        sandbox.addArm();
        const QModelIndex arm = sandbox.index(0);
        CHECK_FALSE(arm.data(Sandbox::JunctionConflictRole).toBool());

        sandbox.setWall(0, Sandbox::ArmJunction, Sandbox::Left, 8.0);
        CHECK(sandbox.junctionCase().arms.at(0).atJunction.left == 8.0);
        CHECK(sandbox.junctionCase().throughAtJunction.left == 1.0);
        CHECK(arm.data(Sandbox::JunctionConflictRole).toBool());
        CHECK(arm.data(Sandbox::JunctionSectionRole).value<QList<double>>() == QList<double> {8.0, 1.0, 1.5, 1.0});

        sandbox.setWall(0, Sandbox::ThroughJunction, Sandbox::Left, 8.0);
        CHECK(sandbox.throughJunction() == QList<double> {8.0, 1.0, 1.5, 1.0});
        CHECK_FALSE(arm.data(Sandbox::JunctionConflictRole).toBool());

        sandbox.setHasJunctionLrud(0, false);
        sandbox.setWall(0, Sandbox::ArmJunction, Sandbox::Up, 9.0);
        CHECK_FALSE(arm.data(Sandbox::JunctionConflictRole).toBool());
    }
}

TEST_CASE("cwPassageTubeSandboxModel copies a case as an initializer that parses back", "[PassageTube][cwPassageTubeSandboxModel]")
{
    using Cases = cwPassageTubeCases;
    using JunctionCase = Cases::JunctionCase;
    using ArmSpec = Cases::ArmSpec;

    SECTION("every gallery case, the wide-arm fork and a sweep case")
    {
        QVector<JunctionCase> cases = Cases::galleryCases();
        cases.append(Cases::wideArmFork());
        cases.append(Cases::sweepCases().constLast());
        for (const JunctionCase& junctionCase : std::as_const(cases)) {
            const QString text = Sandbox::toInitializer(junctionCase);
            const std::optional<JunctionCase> parsed = Sandbox::fromInitializer(text);
            INFO(text.toStdString());
            REQUIRE(parsed.has_value());
            CHECK(*parsed == junctionCase);
        }
    }

    SECTION("as the C++ that compiles to the same case")
    {
        // The literal below is the text of expectedText, compiled.
        const JunctionCase compiled = JunctionCase {
            .name = QStringLiteral("Sandbox \"quoted\""),
            .tags = {QStringLiteral("gallery")},
            .throughSection = {1, 1, 1.5, 1},
            .throughAtJunction = {2, 1, 1.5, 0.1},
            .throughPastJunction = {1, 1, 1.5, 1},
            .throughBendDegrees = 40,
            .throughProfileDegrees = -90,
            .throughBendProfileDegrees = 0,
            .arms = {
                ArmSpec {
                    .planDegrees = 30,
                    .profileDegrees = 0,
                    .lengthMeters = 3,
                    .at = {1, 1, 1.5, 1},
                    .atJunction = {1, 1, 1.5, 1},
                    .hasJunctionLrud = false,
                    .hasLrud = true,
                    .legsBeyond = {{45, 0, 3.25, {3, 2, 3.5, 2}}},
                },
            },
            .sampleSpacing = 0.5,
        };
        const QString expectedText = QStringLiteral(R"cpp(JunctionCase {
    .name = QStringLiteral("Sandbox \"quoted\""),
    .tags = {QStringLiteral("gallery")},
    .throughSection = {1, 1, 1.5, 1},
    .throughAtJunction = {2, 1, 1.5, 0.1},
    .throughPastJunction = {1, 1, 1.5, 1},
    .throughBendDegrees = 40,
    .throughProfileDegrees = -90,
    .throughBendProfileDegrees = 0,
    .arms = {
        ArmSpec {
            .planDegrees = 30,
            .profileDegrees = 0,
            .lengthMeters = 3,
            .at = {1, 1, 1.5, 1},
            .atJunction = {1, 1, 1.5, 1},
            .hasJunctionLrud = false,
            .hasLrud = true,
            .legsBeyond = {{45, 0, 3.25, {3, 2, 3.5, 2}}},
        },
    },
    .sampleSpacing = 0.5,
})cpp");
        CHECK(Sandbox::toInitializer(compiled).toStdString() == expectedText.toStdString());
        CHECK(Sandbox::fromInitializer(expectedText) == std::optional<JunctionCase>(compiled));
    }

    SECTION("text that is no initializer reads as nothing")
    {
        CHECK_FALSE(Sandbox::fromInitializer(QStringLiteral("JunctionCase {.name = 3}")).has_value());
        CHECK_FALSE(Sandbox::fromInitializer(QStringLiteral("JunctionCase {")).has_value());
        CHECK_FALSE(Sandbox::fromInitializer(QString()).has_value());
    }
}

TEST_CASE("cwPassageTubeGalleryModel builds the selected case", "[PassageTube][cwPassageTubeGalleryModel]")
{
    cwScene scene;
    Model model;
    model.setScene(&scene);
    REQUIRE(model.tube() != nullptr);
    REQUIRE(model.linePlot() != nullptr);

    const int row = model.indexOf(kSymmetricT);
    REQUIRE(row >= 0);
    CHECK(nameAt(model, row).startsWith(QStringLiteral("1. Symmetric T")));

    QSignalSpy currentSpy(&model, &Model::currentChanged);
    model.setCurrentIndex(row);
    CHECK(currentSpy.count() == 1);

    CHECK(model.currentName() == nameAt(model, row));
    CHECK_FALSE(model.mesh().isEmpty());
    CHECK(model.reportText().isEmpty() == model.report().issues.isEmpty());
    CHECK(model.issueTexts().size() == model.report().issues.size());
    CHECK(model.issuePositions().size() == model.report().issues.size());
    CHECK(model.data(model.index(row), Model::IssueCountRole).toInt() == model.report().issues.size());

    CHECK(model.tube()->positions() == model.mesh().positions);
    CHECK_FALSE(model.linePlot()->points().isEmpty());

    CHECK(model.junction() == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(model.boundsRadius() > 0.0);
    CHECK(qFuzzyCompare(model.armDirection().length(), 1.0f));
    CHECK(qFuzzyIsNull(model.armDirection().z()));
}

TEST_CASE("cwPassageTubeGalleryModel appends random surveys", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    const int rowsBefore = model.rowCount();
    QSignalSpy countSpy(&model, &Model::countChanged);

    model.addRandom(7, 3);

    REQUIRE(model.rowCount() == rowsBefore + 1);
    CHECK(countSpy.count() == 1);
    const QString name = nameAt(model, rowsBefore);
    CHECK(name.contains(QStringLiteral("seed 7")));

    model.setCurrentIndex(rowsBefore);
    CHECK_FALSE(model.mesh().isEmpty());

    SECTION("random rows survive a catalog reload")
    {
        model.setIncludeSweep(true);
        CHECK(nameAt(model, model.rowCount() - 1) == name);
    }
}

TEST_CASE("cwPassageTubeGalleryModel counts no issues on a junction-free case", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    model.setIncludeSweep(true);

    const int row = model.indexOf(kJunctionFreeCase);
    REQUIRE(row >= 0);
    CHECK(model.data(model.index(row), Model::IssueCountRole).toInt() == 0);
    CHECK(model.data(model.index(row), Model::IssueSummaryRole).toString().isEmpty());
}

// Case 25 turns twice: level into the shaft at f1 and out of it at f2.
TEST_CASE("cwPassageTubeGalleryModel draws the builder's rings", "[PassageTube][cwPassageTubeGalleryModel]")
{
    cwScene scene;
    Model model;
    model.setScene(&scene);
    REQUIRE(model.ringPlot() != nullptr);

    const int row = model.indexOf(kShaftFootCase);
    REQUIRE(row >= 0);
    model.setCurrentIndex(row);

    const Model::RingGeometry geometry = model.ringGeometry();
    const QVector<cwPassageTubeBuilder::Ring> rings = caseRings(kShaftFootCase);
    REQUIRE_FALSE(rings.isEmpty());
    REQUIRE(geometry.loops.size() == rings.size());
    REQUIRE(geometry.rights.count == 2 * rings.size());
    REQUIRE(geometry.ups.count == 2 * rings.size());
    CHECK(model.ringPlot()->points() == geometry.points);

    for (int i = 0; i < rings.size(); ++i) {
        INFO("ring " << i);
        const cwPassageTubeBuilder::Ring& ring = rings.at(i);
        const cwLinePlotGeometry::VertexRange& loop = geometry.loops.at(i);
        const int vertexCount = static_cast<int>(ring.vertices.size());
        REQUIRE(vertexCount > 0);
        if (ring.isGrown) {
            CHECK(loop.count == 2 * ((vertexCount + 1) / 2));
        } else {
            REQUIRE(loop.count == 2 * vertexCount);
            CHECK(geometry.points.at(loop.start) == ring.vertices.constFirst());
            CHECK(geometry.points.at(loop.start + loop.count - 1) == ring.vertices.constFirst());
        }

        const double reach = Model::kFrameAxisMeters;
        const QVector3D rightStart = geometry.points.at(geometry.rights.start + 2 * i);
        const QVector3D rightEnd = geometry.points.at(geometry.rights.start + 2 * i + 1);
        const QVector3D upStart = geometry.points.at(geometry.ups.start + 2 * i);
        const QVector3D upEnd = geometry.points.at(geometry.ups.start + 2 * i + 1);
        CHECK(rightStart == ring.center);
        CHECK(upStart == ring.center);
        CHECK(std::abs(rightStart.distanceToPoint(rightEnd) - reach) < kLengthTolerance);
        CHECK(std::abs(upStart.distanceToPoint(upEnd) - reach) < kLengthTolerance);
        CHECK(QVector3D::dotProduct((upEnd - upStart).normalized(), ring.frame.up) > 1.0f - kLengthTolerance);
    }

    SECTION("the report lists no elbow")
    {
        const QString report = model.reportText();
        INFO(report.toStdString());
        CHECK_FALSE(report.contains(QStringLiteral("elbow at ")));
    }

    SECTION("a ring line names the nearest ring to a position")
    {
        const int nearest = model.nearestRing(rings.constFirst().center);
        CHECK(nearest == 0);
        CHECK(model.ringLine(nearest).startsWith(QStringLiteral("chain 0 ring 0: ")));
        CHECK(model.ringLine(static_cast<int>(rings.size())).isEmpty());
    }
}

// Case 23 reads c0 and c1 and leaves c2 unread; every ring lies on its leg's
// loft or its station's sections, so the overshoot layer, which follows the
// checker's WallBeyondReading, is empty.
TEST_CASE("cwPassageTubeGalleryModel draws the readings each ring was fitted to", "[PassageTube][cwPassageTubeGalleryModel]")
{
    constexpr int kReadStations = 2;
    constexpr int kSpokesPerStation = 4;
    cwScene scene;
    Model model;
    model.setScene(&scene);

    const int row = model.indexOf(kSharpBendCase);
    REQUIRE(row >= 0);
    model.setCurrentIndex(row);

    const Model::RingGeometry geometry = model.ringGeometry();
    const QVector<cwPassageTubeBuilder::Ring> rings = caseRings(kSharpBendCase);
    REQUIRE_FALSE(rings.isEmpty());
    const QVector3D c0 = casePosition(kSharpBendCase, QStringLiteral("c0"));
    const QVector3D c1 = casePosition(kSharpBendCase, QStringLiteral("c1"));
    CHECK(model.ringPlot()->points() == geometry.points);

    SECTION("four spokes from each read station")
    {
        REQUIRE(geometry.readings.count == 2 * kSpokesPerStation * kReadStations);
        const QVector<QVector3D> stations {c0, c1};
        for (int spoke = 0; spoke < geometry.readings.count / 2; ++spoke) {
            const QVector3D start = geometry.points.at(geometry.readings.start + 2 * spoke);
            const QVector3D end = geometry.points.at(geometry.readings.start + 2 * spoke + 1);
            CHECK(std::any_of(stations.cbegin(), stations.cend(), [&](const QVector3D& station) { return isNear(start, station); }));
            CHECK(start.distanceToPoint(end) > 0.0f);
        }
        CHECK(geometry.passageReadings.start == geometry.readings.start + geometry.readings.count);
    }

    SECTION("a loop per sectioned ring")
    {
        const qsizetype sectioned = std::count_if(rings.cbegin(), rings.cend(), [](const cwPassageTubeBuilder::Ring& ring) {
            return ring.section.has_value();
        });
        REQUIRE(sectioned > 0);
        REQUIRE(geometry.sectionLoops.size() == sectioned);
        int loop = 0;
        for (const cwPassageTubeBuilder::Ring& ring : rings) {
            if (ring.section) {
                const cwLinePlotGeometry::VertexRange& range = geometry.sectionLoops.at(loop++);
                CHECK(range.count == 2 * ring.section->stationRadii.size());
                CHECK(geometry.points.at(range.start) == geometry.points.at(range.start + range.count - 1));
            }
        }
        const cwLinePlotGeometry::VertexRange& lastLoop = geometry.sectionLoops.constLast();
        CHECK(geometry.overshoots.start == lastLoop.start + lastLoop.count);
        CHECK(geometry.overshoots.count == 0);
    }

    SECTION("the report names no wall past the readings and the ring line names the corner")
    {
        const QString report = model.reportText();
        INFO(report.toStdString());
        CHECK_FALSE(report.contains(cwPassageTubeMeshCheck::typeName(cwPassageTubeMeshCheck::WallBeyondReading)));
        CHECK_FALSE(report.contains(QStringLiteral(" m past the readings at ")));

        const int middle = model.nearestRing(c1);
        const QString line = model.ringLine(middle);
        INFO(line.toStdString());
        CHECK(line.contains(QStringLiteral("section L/R/U/D ")));
        CHECK(line.contains(QStringLiteral(" around c1 ")));
        CHECK(line.contains(QStringLiteral(" m past the readings at ")));
    }

    SECTION("each layer shows on its own")
    {
        model.setReadingsVisible(true);
        const QVector<quint8> visibility = model.ringPlot()->visibility();
        CHECK(visibility.at(geometry.readings.start) == cwRenderLinePlot::kVisible);
        CHECK(visibility.at(geometry.sectionLoops.constFirst().start) == cwRenderLinePlot::kHidden);
        CHECK(visibility.at(0) == cwRenderLinePlot::kHidden);
    }

    SECTION("framing a station takes in its rings' sections")
    {
        model.setStation(QStringLiteral("c1"));
        const QBox3D withoutSections = model.framingBox();
        model.setSectionsVisible(true);
        const QBox3D box = model.framingBox();
        CHECK(box.contains(withoutSections));
        for (const cwPassageTubeBuilder::Ring& ring : rings) {
            if (ring.section && isNear(ring.section->station, c1)) {
                for (const QVector3D& vertex : ring.vertices) {
                    CHECK(box.contains(vertex));
                }
            }
        }
    }
}

TEST_CASE("cwPassageTubeGalleryModel colors each ring plot layer on its own", "[PassageTube][cwPassageTubeGalleryModel]")
{
    using Range = cwLinePlotGeometry::VertexRange;
    cwScene scene;
    Model model;
    model.setScene(&scene);

    const int row = model.indexOf(kSharpBendCase);
    REQUIRE(row >= 0);
    model.setCurrentIndex(row);

    const Model::RingGeometry geometry = model.ringGeometry();
    const QVector<quint32> types = model.ringPlot()->segmentTypes();
    REQUIRE(types.size() == geometry.points.size() / 2);

    const auto checkType = [&](const Range& range, quint32 type) {
        for (int vertex = range.start; vertex < range.start + range.count; vertex += 2) {
            INFO("vertex " << vertex);
            CHECK(types.at(vertex / 2) == type);
        }
    };
    REQUIRE_FALSE(geometry.loops.isEmpty());
    for (const Range& loop : geometry.loops) {
        checkType(loop, cwRenderLinePlot::kRingSegment);
    }
    REQUIRE(geometry.rights.count > 0);
    checkType(geometry.rights, cwRenderLinePlot::kFrameSegment);
    checkType(geometry.ups, cwRenderLinePlot::kFrameSegment);
    REQUIRE(geometry.readings.count > 0);
    checkType(geometry.readings, cwRenderLinePlot::kReadingSegment);
    checkType(geometry.passageReadings, cwRenderLinePlot::kReadingSegment);
    REQUIRE_FALSE(geometry.sectionLoops.isEmpty());
    for (const Range& loop : geometry.sectionLoops) {
        checkType(loop, cwRenderLinePlot::kSectionSegment);
    }
    checkType(geometry.overshoots, cwRenderLinePlot::kOvershootSegment);

    SECTION("each layer's swatch is its segment color")
    {
        CHECK(model.layerColor(QStringLiteral("rings")) == cwRenderLinePlot::segmentColor(cwRenderLinePlot::kRingSegment));
        CHECK(model.layerColor(QStringLiteral("frames")) == cwRenderLinePlot::segmentColor(cwRenderLinePlot::kFrameSegment));
        CHECK(model.layerColor(QStringLiteral("readings")) == cwRenderLinePlot::segmentColor(cwRenderLinePlot::kReadingSegment));
        CHECK(model.layerColor(QStringLiteral("sections")) == cwRenderLinePlot::segmentColor(cwRenderLinePlot::kSectionSegment));
        CHECK(model.layerColor(QStringLiteral("overshoot")) == cwRenderLinePlot::segmentColor(cwRenderLinePlot::kOvershootSegment));
        CHECK_FALSE(model.layerColor(QStringLiteral("centerline")).isValid());
    }
}

TEST_CASE("cwPassageTubeGalleryModel opens a CaveWhere project", "[PassageTube][cwPassageTubeGalleryModel]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = writeBendProject(QDir(directory.path()));
    const QDateTime savedAt = QFileInfo(path).lastModified();
    const QStringList savedFiles = QDir(QFileInfo(path).absolutePath()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);

    Model model;
    const int rowsBefore = model.rowCount();
    QSignalSpy openedSpy(&model, &Model::projectOpened);
    QSignalSpy failedSpy(&model, &Model::projectFailed);
    model.openProject(path, cwPassageTubeBuilder::Options().sampleSpacingMeters);
    openedSpy.wait(kProjectLoadTimeoutMs);
    INFO((failedSpy.isEmpty() ? QString() : failedSpy.constFirst().constFirst().toString()).toStdString());
    REQUIRE(failedSpy.isEmpty());
    REQUIRE(openedSpy.count() == 1);
    CHECK(openedSpy.constFirst().at(0).toInt() == rowsBefore);
    CHECK(openedSpy.constFirst().at(1).toInt() == kCaveNames.size());

    REQUIRE(model.rowCount() == rowsBefore + kCaveNames.size());
    for (int i = 0; i < kCaveNames.size(); ++i) {
        CHECK(nameAt(model, rowsBefore + i) == kProjectBaseName + QStringLiteral(": ") + kCaveNames.at(i));
        CHECK(model.data(model.index(rowsBefore + i), Model::TagsRole).toString() == QStringLiteral("project"));
    }

    CHECK(QFileInfo(path).lastModified() == savedAt);
    CHECK(QDir(QFileInfo(path).absolutePath()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot) == savedFiles);

    model.setCurrentIndex(rowsBefore);
    REQUIRE_FALSE(model.mesh().isEmpty());

    SECTION("the report matches the equivalent catalog survey")
    {
        const cwPassageTubeCases::Survey survey = cwPassageTubeCases::toSurvey(bendTrip());
        const cwPassageTubeBuilder::Debug debug = cwPassageTubeBuilder::buildDebug(survey.region, survey.positionsByCave, {}, {});
        const cwPassageTubeMeshCheck::Report expected = cwPassageTubeMeshCheck::check(debug.mesh, debug);
        const cwPassageTubeMeshCheck::Report report = model.report();

        CHECK(model.mesh().positions.size() == debug.mesh.positions.size());
        CHECK(model.mesh().indices.size() == debug.mesh.indices.size());
        REQUIRE(report.issues.size() == expected.issues.size());
        REQUIRE(report.warnings.size() == expected.warnings.size());
        for (int i = 0; i < expected.issues.size(); ++i) {
            CHECK(report.issues.at(i).type == expected.issues.at(i).type);
            CHECK(isNear(report.issues.at(i).position, expected.issues.at(i).position));
        }
        for (int i = 0; i < expected.warnings.size(); ++i) {
            CHECK(report.warnings.at(i).type == expected.warnings.at(i).type);
        }
    }

    SECTION("a station frames its legs and looks down the longest")
    {
        const QBox3D wholeSurvey = model.framingBox();
        model.setStation(QStringLiteral("A2"));

        const QBox3D box = model.framingBox();
        CHECK(box.contains(kA2));
        CHECK(box.contains(kA1));
        CHECK(box.contains(kA3));
        CHECK(isNear(model.junction(), kA2));
        CHECK(isNear(model.armFarEnd(), kA3));
        CHECK(isNear(model.armDirection(), QVector3D(-1.0f, 0.0f, 0.0f)));

        model.setStation(QString());
        CHECK(model.framingBox() == wholeSurvey);
    }
}

TEST_CASE("cwPassageTubeGalleryModel names the station nearest each issue", "[PassageTube][cwPassageTubeGalleryModel]")
{
    Model model;
    const int row = model.indexOf(kCaseWithIssues);
    REQUIRE(row >= 0);
    model.setCurrentIndex(row);

    const cwPassageTubeMeshCheck::Report report = model.report();
    const QStringList texts = model.issueTexts();
    REQUIRE_FALSE(texts.isEmpty());
    REQUIRE(texts.size() == report.issues.size());
    for (int i = 0; i < texts.size(); ++i) {
        INFO(texts.at(i).toStdString());
        const QString prefix = cwPassageTubeMeshCheck::typeName(report.issues.at(i).type) + QStringLiteral(" at ");
        REQUIRE(texts.at(i).startsWith(prefix));
        CHECK_FALSE(texts.at(i).mid(prefix.size()).startsWith(QLatin1Char('(')));
        CHECK(model.reportText().contains(texts.at(i)));
    }
}

TEST_CASE("cwPassageTubeGalleryModel reports a project it cannot copy", "[PassageTube][cwPassageTubeGalleryModel]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    Model model;
    const int rowsBefore = model.rowCount();
    QSignalSpy failedSpy(&model, &Model::projectFailed);
    model.openProject(QDir(directory.path()).filePath(QStringLiteral("missing.cwproj")),
                      cwPassageTubeBuilder::Options().sampleSpacingMeters);
    CHECK(failedSpy.count() == 1);
    CHECK(model.rowCount() == rowsBefore);
}
