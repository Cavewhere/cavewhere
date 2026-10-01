/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// C4.3 of plans/SURVEY_TREE_PLAN.html: every equate lives in the region's one
// list. A project saved before that kept within-cave ties on each node's file
// (Cave.equates, field 13); loading it moves them into the region list, and the
// next save writes them to the project file and drops field 13 from the nodes.

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cavewhere.pb.h"
#include "cwCave.h"
#include "cwCavernRunner.h"
#include "cwCavingRegion.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwProtoUtils.h"
#include "cwRootData.h"
#include "cwStationHandle.h"
#include "cwSurvex3DFileReader.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"
#include "ProjectFilenameTestHelper.h"

// Qt
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>

// Protobuf
#include <google/protobuf/util/json_util.h>

namespace {

// The JSON key the pre-C4.3 writer used for Cave.equates (field 13).
const QString kLegacyEquatesKey = QStringLiteral("equates");

// The JSON key of Project.equates, the region list.
const QString kProjectEquatesKey = QStringLiteral("equates");

constexpr double kPositionToleranceMeters = 0.001;

// Wide enough that a rewrite is a different mtime on any filesystem the suite runs on.
constexpr unsigned long kMtimeGapMs = 50;

QJsonObject readJsonObject(const QString& path)
{
    const QJsonDocument document = QJsonDocument::fromJson(fileContents(path));
    REQUIRE(document.isObject());
    return document.object();
}

void writeJsonObject(const QString& path, const QJsonObject& object)
{
    overwriteFile(path, QJsonDocument(object).toJson());
}

QJsonValue equateJson(const cwEquate& equate)
{
    CavewhereProto::Equate proto;
    cwProtoUtils::saveEquate(&proto, equate);
    std::string json;
    REQUIRE(google::protobuf::util::MessageToJsonString(proto, &json).ok());
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    REQUIRE(document.isObject());
    return document.object();
}

bool fileHoldsLegacyEquates(const QString& path)
{
    return readJsonObject(path).contains(kLegacyEquatesKey);
}

int fileVersionOf(const QString& path)
{
    return readJsonObject(path).value(QStringLiteral("fileVersion")).toObject()
            .value(QStringLiteral("version")).toInt();
}

void settle(cwRootData* rootData)
{
    rootData->project()->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
}

std::unique_ptr<cwRootData> openProject(const QString& projectPath)
{
    auto rootData = std::make_unique<cwRootData>();
    rootData->project()->loadFile(projectPath);
    rootData->project()->waitLoadToFinish();
    settle(rootData.get());
    return rootData;
}

cwCave* findNode(const cwCavingRegion* region, const QUuid& id)
{
    for (cwSurveyNode* node : region->rootNode()->allNodes()) {
        if (node->id() == id) {
            return qobject_cast<cwCave*>(node);
        }
    }
    return nullptr;
}

cwStationHandle handleOf(const cwTrip* trip, const QString& tail)
{
    const cwStationHandle handle = trip->stationHandle(tail);
    REQUIRE(handle.isValid());
    return handle;
}

//! A saved project whose cave and whose child node each carry one within-node
//! tie in field 13, the way a project saved before C4.3 holds them.
struct LegacyProject {
    std::unique_ptr<SavedProjectFixture> fixture;
    QString projectPath;
    QUuid caveId;
    QUuid childId;
    cwEquate caveEquate;
    cwEquate childEquate;
    cwSurvex3DFileReader::NetworkAndLookup solvedBefore;
};

//! A path in the fixture's temporary directory, beside the project directory
//! rather than in it, so a solve leaves the project's files as they were.
QString scratchFile(const LegacyProject& legacy, const QString& fileName)
{
    return QDir(legacy.fixture->tempDir.path()).absoluteFilePath(fileName);
}

cwSurvex3DFileReader::NetworkAndLookup solve(const cwCavingRegion* region,
                                              const QString& driverPath)
{
    const auto exported = cwSurvexExporterRegion::exportRegion(region->data(), driverPath);
    REQUIRE_FALSE(exported.hasError());
    const QString threeDPath = driverPath + QStringLiteral(".3d");
    const auto ran = cwCavernRunner::run(driverPath, threeDPath);
    INFO("cavern: " << ran.errorMessage().toStdString());
    REQUIRE_FALSE(ran.hasError());
    cwSurvex3DFileReader reader;
    return reader.readNetworkAndLookup(threeDPath);
}

LegacyProject makeLegacyProject(const QString& projectFileBase)
{
    LegacyProject legacy;
    legacy.fixture = makeSavedProject(projectFileBase,
                                      QStringLiteral("Alpha"),
                                      QStringLiteral("Empty"));
    cwCave* cave = legacy.fixture->cave;
    const cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                                QStringLiteral("A1"), QStringLiteral("A2"));
    const cwTrip* second = addNativeTripWithShot(cave, QStringLiteral("Second"),
                                                 QStringLiteral("B1"), QStringLiteral("B2"));
    cwCave* child = addChildNode(cave, QStringLiteral("Upper level"), cwSurveyNode::Kind::Cave);
    const cwTrip* upper = addNativeTripWithShot(child, QStringLiteral("Upper"),
                                                QStringLiteral("U1"), QStringLiteral("U2"));
    const cwTrip* upperSide = addNativeTripWithShot(child, QStringLiteral("Upper side"),
                                                    QStringLiteral("V1"), QStringLiteral("V2"));
    settle(legacy.fixture->rootData.get());

    legacy.projectPath = legacy.fixture->project->filename();
    legacy.caveId = cave->id();
    legacy.childId = child->id();
    legacy.caveEquate = cwEquate({handleOf(first, QStringLiteral("A2")),
                                  handleOf(second, QStringLiteral("B1"))});
    legacy.childEquate = cwEquate({handleOf(upper, QStringLiteral("U2")),
                                   handleOf(upperSide, QStringLiteral("V1"))});
    REQUIRE(cave->validate(legacy.caveEquate));
    REQUIRE(child->validate(legacy.childEquate));

    // What the user tied, solved: the reference the migrated project must match.
    auto* region = legacy.fixture->project->cavingRegion();
    region->equates()->setEquates({legacy.caveEquate, legacy.childEquate});
    legacy.solvedBefore = solve(region, scratchFile(legacy, QStringLiteral("before.svx")));
    REQUIRE_FALSE(legacy.solvedBefore.lookup.positions().isEmpty());
    settle(legacy.fixture->rootData.get());

    // Close the session before rewriting its files, so nothing it still has
    // queued lands on top of the legacy layout.
    legacy.fixture->rootData.reset();
    legacy.fixture->project = nullptr;
    legacy.fixture->cave = nullptr;
    legacy.fixture->trip = nullptr;

    REQUIRE(QFileInfo::exists(legacy.projectPath));
    // A pre-C4.3 project keeps no region list for these ties.
    QJsonObject projectObject = readJsonObject(legacy.projectPath);
    projectObject.remove(kProjectEquatesKey);
    writeJsonObject(legacy.projectPath, projectObject);

    auto rootData = openProject(legacy.projectPath);
    const cwCavingRegion* reopened = rootData->project()->cavingRegion();
    const cwCave* reopenedCave = findNode(reopened, legacy.caveId);
    const cwCave* reopenedChild = findNode(reopened, legacy.childId);
    REQUIRE(reopenedCave != nullptr);
    REQUIRE(reopenedChild != nullptr);
    const QString cavePath = ProjectFilenameTestHelper::absolutePath(reopenedCave);
    const QString childPath = ProjectFilenameTestHelper::absolutePath(reopenedChild);
    rootData.reset();

    QJsonObject caveObject = readJsonObject(cavePath);
    caveObject.insert(kLegacyEquatesKey, QJsonArray({equateJson(legacy.caveEquate)}));
    writeJsonObject(cavePath, caveObject);
    QJsonObject childObject = readJsonObject(childPath);
    childObject.insert(kLegacyEquatesKey, QJsonArray({equateJson(legacy.childEquate)}));
    writeJsonObject(childPath, childObject);

    return legacy;
}

void checkSameSolve(const cwSurvex3DFileReader::NetworkAndLookup& expected,
                    const cwSurvex3DFileReader::NetworkAndLookup& actual)
{
    const QMap<QString, QVector3D> expectedPositions = expected.lookup.positions();
    const QMap<QString, QVector3D> actualPositions = actual.lookup.positions();
    CHECK(actualPositions.keys() == expectedPositions.keys());
    for (auto it = expectedPositions.cbegin(); it != expectedPositions.cend(); ++it) {
        INFO("station " << it.key().toStdString());
        REQUIRE(actualPositions.contains(it.key()));
        CHECK((actualPositions.value(it.key()) - it.value()).length()
              == Catch::Approx(0.0).margin(kPositionToleranceMeters));
    }

    QStringList expectedStations = expected.network.stations();
    QStringList actualStations = actual.network.stations();
    expectedStations.sort();
    actualStations.sort();
    CHECK(actualStations == expectedStations);
    for (const QString& station : std::as_const(expectedStations)) {
        INFO("legs of " << station.toStdString());
        QStringList expectedNeighbors = expected.network.neighbors(station);
        QStringList actualNeighbors = actual.network.neighbors(station);
        expectedNeighbors.sort();
        actualNeighbors.sort();
        CHECK(actualNeighbors == expectedNeighbors);
    }
}

} // namespace

TEST_CASE("Within-node equates saved on their nodes load into the region list",
          "[Equate][Migration]")
{
    LegacyProject legacy = makeLegacyProject(QStringLiteral("equate-migration-load"));

    const QString projectPath = legacy.projectPath;
    const QDateTime projectMtime = QFileInfo(projectPath).lastModified();
    QThread::msleep(kMtimeGapMs);

    auto rootData = openProject(projectPath);
    const cwCavingRegion* region = rootData->project()->cavingRegion();
    const cwCave* cave = findNode(region, legacy.caveId);
    const cwCave* child = findNode(region, legacy.childId);
    REQUIRE(cave != nullptr);
    REQUIRE(child != nullptr);

    // Every node in the walk hands its ties over, the nested one included.
    REQUIRE(region->equates()->count() == 2);
    CHECK(region->equates()->equates().contains(legacy.caveEquate));
    CHECK(region->equates()->equates().contains(legacy.childEquate));

    // Opening the project writes nothing: the files keep their legacy shape
    // until the user changes something.
    const QString cavePath = ProjectFilenameTestHelper::absolutePath(cave);
    const QString childPath = ProjectFilenameTestHelper::absolutePath(child);
    CHECK(QFileInfo(projectPath).lastModified() == projectMtime);
    CHECK(fileHoldsLegacyEquates(cavePath));
    CHECK(fileHoldsLegacyEquates(childPath));

    checkSameSolve(legacy.solvedBefore,
                   solve(region, scratchFile(legacy, QStringLiteral("migrated.svx"))));
}

TEST_CASE("The first save after a migration moves the ties to the project file",
          "[Equate][Migration]")
{
    LegacyProject legacy = makeLegacyProject(QStringLiteral("equate-migration-save"));

    QString cavePath;
    QString childPath;
    int caveVersion = 0;
    int projectVersion = 0;
    {
        auto rootData = openProject(legacy.projectPath);
        cwCavingRegion* region = rootData->project()->cavingRegion();
        cwCave* cave = findNode(region, legacy.caveId);
        cwCave* child = findNode(region, legacy.childId);
        REQUIRE(cave != nullptr);
        REQUIRE(child != nullptr);
        cavePath = ProjectFilenameTestHelper::absolutePath(cave);
        childPath = ProjectFilenameTestHelper::absolutePath(child);
        caveVersion = fileVersionOf(cavePath);
        projectVersion = fileVersionOf(legacy.projectPath);

        // An edit that rewrites only the cave's own file.
        cave->length()->setUnit(cwUnits::Feet);
        settle(rootData.get());

        CHECK_FALSE(fileHoldsLegacyEquates(cavePath));
        CHECK_FALSE(fileHoldsLegacyEquates(childPath));
        CHECK(fileVersionOf(cavePath) == caveVersion);
        CHECK(fileVersionOf(legacy.projectPath) == projectVersion);
    }

    // The ties were written to the project file in the same save that took them
    // off the nodes, so none is lost and none is read twice.
    auto rootData = openProject(legacy.projectPath);
    const cwCavingRegion* region = rootData->project()->cavingRegion();
    REQUIRE(region->equates()->count() == 2);
    CHECK(region->equates()->equates().contains(legacy.caveEquate));
    CHECK(region->equates()->equates().contains(legacy.childEquate));

    checkSameSolve(legacy.solvedBefore,
                   solve(region, scratchFile(legacy, QStringLiteral("resaved.svx"))));
}

TEST_CASE("A migrated equate the user removes stays removed",
          "[Equate][Migration]")
{
    LegacyProject legacy = makeLegacyProject(QStringLiteral("equate-migration-remove"));

    {
        auto rootData = openProject(legacy.projectPath);
        cwCavingRegion* region = rootData->project()->cavingRegion();
        REQUIRE(region->equates()->count() == 2);
        const int caveEquateRow = region->equates()->equates().indexOf(legacy.caveEquate);
        REQUIRE(caveEquateRow >= 0);
        region->equates()->removeAt(caveEquateRow);
        settle(rootData.get());
    }

    // Saving the shorter region list also takes the tie off the node's file,
    // which would otherwise hand it back on the next open.
    auto rootData = openProject(legacy.projectPath);
    const cwCavingRegion* region = rootData->project()->cavingRegion();
    REQUIRE(region->equates()->count() == 1);
    CHECK(region->equates()->equateAt(0) == legacy.childEquate);
}

TEST_CASE("A tie already in the region list is read once from both files",
          "[Equate][Migration]")
{
    LegacyProject legacy = makeLegacyProject(QStringLiteral("equate-migration-dedup"));

    // A project file that already carries the cave's tie, beside a cave file
    // that still does: the state a copy of the project made mid-migration has.
    QJsonObject projectObject = readJsonObject(legacy.projectPath);
    projectObject.insert(kProjectEquatesKey, QJsonArray({equateJson(legacy.caveEquate)}));
    writeJsonObject(legacy.projectPath, projectObject);

    auto rootData = openProject(legacy.projectPath);
    const cwCavingRegion* region = rootData->project()->cavingRegion();
    CHECK(region->equates()->count() == 2);
}
