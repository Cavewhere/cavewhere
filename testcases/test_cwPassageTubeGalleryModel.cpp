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
#include "cwPassageTubeCases.h"
#include "cwPassageTubeGalleryModel.h"
#include "cwProject.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPassageTube.h"
#include "cwScene.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Qt includes
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

//Std includes
#include <algorithm>
#include <cmath>

namespace {

using Model = cwPassageTubeGalleryModel;

const QString kSymmetricT = QStringLiteral("1. symmetric T");
const QString kJunctionFreeCase = QStringLiteral("no arms, through profile 0, bend 0, same section");
const QString kShaftFootCase = QStringLiteral("25. Zero floor reading at the foot of a shaft");
const QString kSharpBendCase = QStringLiteral("23. Sharp bend at a station reading zero on the outside wall");
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

    SECTION("without the sweep, the catalog's hand-made cases in order")
    {
        QStringList expected;
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

    SECTION("with the sweep, every catalog case")
    {
        model.setIncludeSweep(true);
        REQUIRE(model.rowCount() == catalog.size());
        CHECK(nameAt(model, model.rowCount() - 1) == catalog.constLast().name);
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

    int elbowRings = 0;
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

        const double reach = Model::kFrameAxisMeters * (ring.isElbow ? Model::kElbowFrameScale : 1.0);
        const QVector3D rightStart = geometry.points.at(geometry.rights.start + 2 * i);
        const QVector3D rightEnd = geometry.points.at(geometry.rights.start + 2 * i + 1);
        const QVector3D upStart = geometry.points.at(geometry.ups.start + 2 * i);
        const QVector3D upEnd = geometry.points.at(geometry.ups.start + 2 * i + 1);
        CHECK(rightStart == ring.center);
        CHECK(upStart == ring.center);
        CHECK(std::abs(rightStart.distanceToPoint(rightEnd) - reach) < kLengthTolerance);
        CHECK(std::abs(upStart.distanceToPoint(upEnd) - reach) < kLengthTolerance);
        CHECK(QVector3D::dotProduct((upEnd - upStart).normalized(), ring.frame.up) > 1.0f - kLengthTolerance);
        if (ring.isElbow) {
            ++elbowRings;
        }
    }
    CHECK(elbowRings > 0);

    SECTION("the report lists each elbow with its roll")
    {
        const QString report = model.reportText();
        INFO(report.toStdString());
        CHECK(report.contains(QStringLiteral("elbow at f2")));
        const QStringList elbowLines = report.split(QLatin1Char('\n')).filter(QStringLiteral("elbow at "));
        REQUIRE_FALSE(elbowLines.isEmpty());
        for (const QString& line : elbowLines) {
            CHECK(line.contains(QStringLiteral("turn ")));
            CHECK(line.contains(QStringLiteral("roll ")));
        }
    }

    SECTION("a ring line names the nearest ring to a position")
    {
        const int nearest = model.nearestRing(rings.constFirst().center);
        CHECK(nearest == 0);
        CHECK(model.ringLine(nearest).startsWith(QStringLiteral("chain 0 ring 0: ")));
        CHECK(model.ringLine(static_cast<int>(rings.size())).isEmpty());
    }
}

// Case 23 reads c0 and c1 and leaves c2 unread; the elbow at c1 keeps
// within c1's readings, so the overshoot layer, which follows the checker's
// WallBeyondReading, is empty.
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

    SECTION("a loop per sectioned ring and an inset per sectioned elbow ring")
    {
        const qsizetype sectioned = std::count_if(rings.cbegin(), rings.cend(), [](const cwPassageTubeBuilder::Ring& ring) {
            return ring.section.has_value();
        });
        const qsizetype sectionedElbows = std::count_if(rings.cbegin(), rings.cend(), [](const cwPassageTubeBuilder::Ring& ring) {
            return ring.section.has_value() && ring.isElbow;
        });
        REQUIRE(sectioned > 0);
        REQUIRE(sectionedElbows > 0);
        REQUIRE(geometry.sectionLoops.size() == sectioned);
        int loop = 0;
        for (const cwPassageTubeBuilder::Ring& ring : rings) {
            if (ring.section) {
                const cwLinePlotGeometry::VertexRange& range = geometry.sectionLoops.at(loop++);
                CHECK(range.count == 2 * ring.section->stationRadii.size());
                CHECK(geometry.points.at(range.start) == geometry.points.at(range.start + range.count - 1));
            }
        }
        CHECK(geometry.sectionInsets.count == 2 * sectionedElbows);
        CHECK(geometry.overshoots.start == geometry.sectionInsets.start + geometry.sectionInsets.count);
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
        CHECK(visibility.at(geometry.sectionInsets.start) == cwRenderLinePlot::kHidden);
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
    const int row = model.indexOf(kShaftFootCase);
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
