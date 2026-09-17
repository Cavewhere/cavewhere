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
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwFixStationModel.h"
#include "cwFutureManagerModel.h"
#include "cwNote.h"
#include "cwErrorListModel.h"
#include "cwProject.h"
#include "cwRegionIOTask.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwStationHandle.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "LoadProjectHelper.h"
#include "ProjectFilenameTestHelper.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"

//QQuickGit includes
#include "LfsPolicy.h"

//Qt includes
#include <algorithm>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUrl>
#include <QUuid>

using namespace SurveyTreeTestHelper;

namespace {

const QString kFolderName = QStringLiteral("Kentucky field seasons");
const QString kSideCaveName = QStringLiteral("Side Cave");
const QString kSectionName = QStringLiteral("Upper level");
const QString kSectionTripName = QStringLiteral("Dome climb");
const QString kFisherRidgeName = QStringLiteral("Fisher Ridge");
const QString kEntranceTripName = QStringLiteral("Entrance survey");
const QString kSecondTripName = QStringLiteral("Crystal crawl");
const QString kSideCaveTripName = QStringLiteral("Sump dig");

//The version a project with no hierarchy is stamped with, so it still opens in
//a build that predates the survey tree.
constexpr int kFlatVersion = 9;

//! The project layout every tree test and the checked-in fixture share:
//!
//!   Fisher Ridge                      (Cave, two trips, one note image)
//!   Kentucky field seasons            (Folder)
//!     nodes/Side Cave                 (Cave, one trip)
//!       nodes/Upper level             (Folder — a Section)
//!         trips/Dome climb
//!   Side Cave                         (Cave, a second top-level node of that name)
struct TreeFixture {
    cwCave* fisherRidge = nullptr;
    cwCave* folder = nullptr;
    cwCave* sideCave = nullptr;
    cwCave* section = nullptr;
    cwCave* topLevelSideCave = nullptr;
};

TreeFixture buildTree(cwRootData* rootData, bool withNoteImage)
{
    auto region = rootData->project()->cavingRegion();

    TreeFixture fixture;
    fixture.fisherRidge = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kFisherRidgeName);
    addTrip(fixture.fisherRidge, kEntranceTripName, QStringLiteral("A"));
    addTrip(fixture.fisherRidge, kSecondTripName, QStringLiteral("B"));

    fixture.folder = addNode(region, nullptr, cwSurveyNode::Kind::Folder, kFolderName);
    fixture.sideCave = addNode(region, fixture.folder, cwSurveyNode::Kind::Cave, kSideCaveName);
    addTrip(fixture.sideCave, kSideCaveTripName, QStringLiteral("C"));

    fixture.section = addNode(region, fixture.sideCave, cwSurveyNode::Kind::Folder, kSectionName);
    addTrip(fixture.section, kSectionTripName, QStringLiteral("D"));

    fixture.topLevelSideCave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kSideCaveName);

    //One cross-node tie, which lives on the region rather than on any node.
    region->equates()->appendEquate(cwEquate({
        cwStationHandle(cwStationHandle::NativeCave, fixture.fisherRidge->id(), QStringLiteral("A2")),
        cwStationHandle(cwStationHandle::NativeCave, fixture.sideCave->id(), QStringLiteral("C1"))
    }));

    if (withNoteImage) {
        const QString noteImagePath = copyToTempFolder(testcasesDatasetPath("test_cwAddImageTask/supportedImage.png"));
        cwTrip* entranceTrip = fixture.fisherRidge->trip(0);
        entranceTrip->notes()->addFromFiles({QUrl::fromLocalFile(noteImagePath)});
        rootData->futureManagerModel()->waitForFinished();
    }

    return fixture;
}

//! The FileVersion stamped in one saved descriptor, which is written as proto
//! JSON, or -1 when the file carries none.
int fileVersionOf(const QString& path)
{
    const QJsonDocument document = QJsonDocument::fromJson(readFile(path));
    if (!document.isObject()) {
        return -1;
    }
    const QJsonObject fileVersion = document.object().value(QStringLiteral("fileVersion")).toObject();
    if (fileVersion.isEmpty()) {
        return -1;
    }
    return fileVersion.value(QStringLiteral("version")).toInt(-1);
}

//! Checks every descriptor in \a projectRootDir carries \a expectedVersion.
void checkEveryFileVersion(const QDir& projectRootDir, int expectedVersion)
{
    const QStringList descriptorSuffixes = {
        QStringLiteral("cwproj"), QStringLiteral("cwcave"), QStringLiteral("cwtrip"),
        QStringLiteral("cwnote"), QStringLiteral("cwnote3d"), QStringLiteral("cwsketch")
    };

    int checked = 0;
    QDirIterator it(projectRootDir.absolutePath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (projectRootDir.relativeFilePath(path).startsWith(QStringLiteral(".git"))
                || !descriptorSuffixes.contains(info.suffix(), Qt::CaseInsensitive)) {
            continue;
        }
        INFO("File " << path.toStdString());
        CHECK(fileVersionOf(path) == expectedVersion);
        checked++;
    }
    CHECK(checked > 0);
}

//! Copies \a source to \a destination, leaving out the git repository the save
//! made for the project: the fixture is checked into CaveWhere's own repository,
//! where a nested repository cannot go and where the project's own LFS
//! .gitattributes would claim every note image in the tree.
void copyDirectory(const QDir& source, const QDir& destination)
{
    REQUIRE(QDir().mkpath(destination.absolutePath()));
    const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        if (entry.fileName() == QStringLiteral(".git")
                || entry.fileName() == QStringLiteral(".gitattributes")) {
            continue;
        }
        const QString destinationPath = destination.absoluteFilePath(entry.fileName());
        if (entry.isDir()) {
            copyDirectory(QDir(entry.absoluteFilePath()), QDir(destinationPath));
        } else {
            REQUIRE(QFile::copy(entry.absoluteFilePath(), destinationPath));
        }
    }
}

//! The one .cwproj file directly inside \a projectRootDir.
QString projectFileIn(const QDir& projectRootDir)
{
    const QStringList files = projectRootDir.entryList(QStringList() << QStringLiteral("*.cwproj"), QDir::Files);
    REQUIRE(files.size() == 1);
    return projectRootDir.absoluteFilePath(files.first());
}

cwCave* childNamed(const cwSurveyNode* parent, const QString& name)
{
    const QList<cwSurveyNode*> children = parent->childNodes();
    for (cwSurveyNode* child : children) {
        if (child->name() == name) {
            return qobject_cast<cwCave*>(child);
        }
    }
    return nullptr;
}

//! The trip of \a node called \a name, nullptr when it holds none: the loader
//! reads trips and child nodes in directory order rather than row order, so a
//! test names what it wants instead of counting rows.
cwTrip* tripNamed(const cwCave* node, const QString& name)
{
    for (int i = 0; i < node->tripCount(); i++) {
        if (node->trip(i)->name() == name) {
            return node->trip(i);
        }
    }
    return nullptr;
}

//! Compares two saved trees the way the loader has to reproduce them: shape,
//! names, ids, kinds, source scalars and trips, matched by id because the
//! loader's order is the directory's.
void compareNodeData(const cwCaveData& expected, const cwCaveData& actual)
{
    INFO("Node " << expected.name.toStdString());
    CHECK(actual.name == expected.name);
    CHECK(actual.id == expected.id);
    CHECK(actual.kind == expected.kind);
    CHECK(actual.readOnly == expected.readOnly);
    CHECK(actual.sourceId == expected.sourceId);
    CHECK(actual.sourcePath == expected.sourcePath);

    REQUIRE(actual.trips.size() == expected.trips.size());
    for (const cwTripData& expectedTrip : expected.trips) {
        const auto actualTrip = std::find_if(actual.trips.constBegin(), actual.trips.constEnd(),
                                             [&expectedTrip](const cwTripData& trip) {
            return trip.id == expectedTrip.id;
        });
        REQUIRE(actualTrip != actual.trips.constEnd());
        CHECK(actualTrip->name == expectedTrip.name);
        CHECK(actualTrip->sourcePath == expectedTrip.sourcePath);
        CHECK(actualTrip->chunks.size() == expectedTrip.chunks.size());
        CHECK(actualTrip->noteModel.notes.size() == expectedTrip.noteModel.notes.size());
    }

    REQUIRE(actual.nodes.size() == expected.nodes.size());
    for (const cwCaveData& expectedChild : expected.nodes) {
        const auto actualChild = std::find_if(actual.nodes.constBegin(), actual.nodes.constEnd(),
                                              [&expectedChild](const cwCaveData& child) {
            return child.id == expectedChild.id;
        });
        REQUIRE(actualChild != actual.nodes.constEnd());
        compareNodeData(expectedChild, *actualChild);
    }
}

}

TEST_CASE("cwSaveLoad writes a node tree as nested nodes directories", "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    const TreeFixture fixture = buildTree(rootData.get(), true);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("node-tree"));

    const QDir dataRoot = rootData->project()->dataRootDir();
    REQUIRE(dataRoot.exists());

    //§6.1: nodes/ is a sibling of trips/, all the way down.
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Fisher Ridge/Fisher Ridge.cwcave"))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Fisher Ridge/trips/Entrance survey/Entrance survey.cwtrip"))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/Kentucky field seasons.cwcave"))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/nodes/Side Cave/Side Cave.cwcave"))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/nodes/Side Cave/trips/Sump dig/Sump dig.cwtrip"))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/nodes/Side Cave/nodes/Upper level/Upper level.cwcave"))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/nodes/Side Cave/nodes/Upper level/trips/Dome climb/Dome climb.cwtrip"))));

    //A second top-level node may carry the same name as a nested one.
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Side Cave/Side Cave.cwcave"))));

    //The depth-one bug: a node added under a cave is written only under nodes/.
    CHECK_FALSE(QFileInfo::exists(dataRoot.absoluteFilePath(kSectionName)));
    CHECK_FALSE(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Side Cave/nodes/Upper level"))));

    CHECK(ProjectFilenameTestHelper::absolutePath(fixture.section)
          == dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/nodes/Side Cave/nodes/Upper level/Upper level.cwcave")));

    //A project with hierarchy is stamped with the tree format's version.
    checkEveryFileVersion(QFileInfo(projectFile).absoluteDir(), cwRegionIOTask::protoVersion());

    const cwCavingRegionData savedRegion = rootData->project()->cavingRegion()->data();

    SECTION("the tree loads back identically")
    {
        auto loadedRoot = std::make_unique<cwRootData>();
        addTokenManager(loadedRoot->project());
        loadedRoot->project()->loadOrConvert(projectFile);
        loadedRoot->project()->waitLoadToFinish();

        auto loadedRegion = loadedRoot->project()->cavingRegion();
        CHECK(loadedRoot->project()->errorModel()->isEmpty());

        const cwCavingRegionData loadedData = loadedRegion->data();
        REQUIRE(loadedData.caves.size() == savedRegion.caves.size());
        for (const cwCaveData& savedCave : savedRegion.caves) {
            const auto loadedCave = std::find_if(loadedData.caves.constBegin(), loadedData.caves.constEnd(),
                                                 [&savedCave](const cwCaveData& cave) {
                return cave.id == savedCave.id;
            });
            REQUIRE(loadedCave != loadedData.caves.constEnd());
            compareNodeData(savedCave, *loadedCave);
        }

        cwCave* loadedFolder = childNamed(loadedRegion->rootNode(), kFolderName);
        REQUIRE(loadedFolder != nullptr);
        CHECK(loadedFolder->kind() == cwSurveyNode::Kind::Folder);
        cwCave* loadedSideCave = childNamed(loadedFolder, kSideCaveName);
        REQUIRE(loadedSideCave != nullptr);
        cwCave* loadedSection = childNamed(loadedSideCave, kSectionName);
        REQUIRE(loadedSection != nullptr);
        REQUIRE(loadedSection->tripCount() == 1);
        CHECK(loadedSection->trip(0)->name() == kSectionTripName);

        //The note image of a depth-one trip still travels with it.
        cwCave* loadedFisherRidge = childNamed(loadedRegion->rootNode(), kFisherRidgeName);
        REQUIRE(loadedFisherRidge != nullptr);
        REQUIRE(loadedFisherRidge->tripCount() == 2);
        cwTrip* loadedEntranceTrip = tripNamed(loadedFisherRidge, kEntranceTripName);
        REQUIRE(loadedEntranceTrip != nullptr);
        CHECK(loadedEntranceTrip->notes()->rowCount() == 1);

        CHECK(loadedRegion->equates()->count() == 1);
    }
}

TEST_CASE("cwSaveLoad prunes trip and attachment subtrees from the node scan", "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    buildTree(rootData.get(), false);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("pruned-scan"));

    const QDir dataRoot = rootData->project()->dataRootDir();

    //A .cwcave planted where only trip files and copied source files live names
    //no node, and the scan never even reads it.
    const QString strayTripsDescriptor =
            dataRoot.absoluteFilePath(QStringLiteral("Fisher Ridge/trips/Entrance survey/Stray.cwcave"));
    const QString attachmentDirPath =
            dataRoot.absoluteFilePath(QStringLiteral("Fisher Ridge/external-centerline"));
    REQUIRE(QDir().mkpath(attachmentDirPath));
    const QString strayAttachmentDescriptor = QDir(attachmentDirPath).absoluteFilePath(QStringLiteral("Stray source.cwcave"));

    const QByteArray strayContent = readFile(dataRoot.absoluteFilePath(QStringLiteral("Side Cave/Side Cave.cwcave")));
    for (const QString& strayPath : {strayTripsDescriptor, strayAttachmentDescriptor}) {
        QFile strayFile(strayPath);
        REQUIRE(strayFile.open(QIODevice::WriteOnly));
        strayFile.write(strayContent);
    }

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    auto loadedRegion = loadedRoot->project()->cavingRegion();
    CHECK(loadedRegion->caveCount() == 3);
    CHECK(childNamed(loadedRegion->rootNode(), QStringLiteral("Stray")) == nullptr);
    CHECK(childNamed(loadedRegion->rootNode(), QStringLiteral("Stray source")) == nullptr);
    CHECK(loadedRoot->project()->errorModel()->isEmpty());
}

TEST_CASE("cwSaveLoad reports a misplaced node descriptor and loads the rest", "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    buildTree(rootData.get(), false);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("misplaced"));

    const QDir dataRoot = rootData->project()->dataRootDir();

    //Two descriptors in one directory: the loader names the second and skips it
    //rather than guessing which node the directory is.
    const QString secondDescriptor = dataRoot.absoluteFilePath(QStringLiteral("Fisher Ridge/Zzz second.cwcave"));
    QFile secondFile(secondDescriptor);
    REQUIRE(secondFile.open(QIODevice::WriteOnly));
    secondFile.write(readFile(dataRoot.absoluteFilePath(QStringLiteral("Side Cave/Side Cave.cwcave"))));
    secondFile.close();

    //A descriptor in a directory that is neither the data root nor a node's
    //nodes/ child is reported too.
    const QString orphanDirPath = dataRoot.absoluteFilePath(QStringLiteral("Fisher Ridge/somewhere else"));
    REQUIRE(QDir().mkpath(orphanDirPath));
    REQUIRE(QFile::copy(secondDescriptor, QDir(orphanDirPath).absoluteFilePath(QStringLiteral("Orphan.cwcave"))));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    auto loadedRegion = loadedRoot->project()->cavingRegion();
    CHECK(loadedRegion->caveCount() == 3);
    CHECK(childNamed(loadedRegion->rootNode(), kFolderName) != nullptr);

    const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
    int reported = 0;
    for (const cwError& error : errors) {
        if (error.message().contains(QStringLiteral("Zzz second.cwcave"))
                || error.message().contains(QStringLiteral("Orphan.cwcave"))) {
            reported++;
        }
    }
    CHECK(reported == 2);
}

TEST_CASE("cwSaveLoad keeps a flat project at file version 9", "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* cave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kFisherRidgeName);
    addTrip(cave, kEntranceTripName, QStringLiteral("A"));
    addTrip(cave, kSecondTripName, QStringLiteral("B"));
    cwCave* secondCave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kSideCaveName);
    addTrip(secondCave, kSideCaveTripName, QStringLiteral("C"));

    QTemporaryDir firstDir;
    REQUIRE(firstDir.isValid());
    const QString firstProjectFile = saveProjectAs(rootData.get(), QDir(firstDir.path()), QStringLiteral("flat"));
    const QDir firstDataRoot = rootData->project()->dataRootDir();

    checkEveryFileVersion(QFileInfo(firstProjectFile).absoluteDir(), kFlatVersion);

    //Written again from the loaded data, a flat project is the same tree of the
    //same bytes: nothing about the tree format leaks into it.
    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(firstProjectFile);
    loadedRoot->project()->waitLoadToFinish();

    auto rewriteRoot = std::make_unique<cwRootData>();
    rewriteRoot->project()->cavingRegion()->setData(loadedRoot->project()->cavingRegion()->data());

    QTemporaryDir secondDir;
    REQUIRE(secondDir.isValid());
    saveProjectAs(rewriteRoot.get(), QDir(secondDir.path()), QStringLiteral("flat"));
    const QDir secondDataRoot = rewriteRoot->project()->dataRootDir();

    const QStringList firstFiles = relativeFiles(firstDataRoot);
    CHECK(relativeFiles(secondDataRoot) == firstFiles);
    for (const QString& relative : firstFiles) {
        INFO("File " << relative.toStdString());
        CHECK(readFile(secondDataRoot.absoluteFilePath(relative))
              == readFile(firstDataRoot.absoluteFilePath(relative)));
    }
}

TEST_CASE("cwSaveLoad loads a node the user named after a layout directory", "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    //Nothing reserves the names the layout uses for its own directories, so a
    //node called "Trips" and a trip called "notes" are ordinary survey data:
    //only where a directory sits says what it holds, never what it is called.
    cwCave* trips = addNode(region, nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Trips"));
    addTrip(trips, QStringLiteral("notes"), QStringLiteral("A"));
    cwCave* nestedNotes = addNode(region, trips, cwSurveyNode::Kind::Cave, QStringLiteral("Notes"));
    addTrip(nestedNotes, QStringLiteral("external-centerline"), QStringLiteral("B"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("reserved-names"));

    const QDir dataRoot = rootData->project()->dataRootDir();
    REQUIRE(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Trips/Trips.cwcave"))));
    REQUIRE(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Trips/trips/notes/notes.cwtrip"))));
    REQUIRE(QFileInfo::exists(dataRoot.absoluteFilePath(QStringLiteral("Trips/nodes/Notes/Notes.cwcave"))));
    REQUIRE(QFileInfo::exists(dataRoot.absoluteFilePath(
                                  QStringLiteral("Trips/nodes/Notes/trips/external-centerline/external-centerline.cwtrip"))));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    auto loadedRegion = loadedRoot->project()->cavingRegion();
    CHECK(loadedRoot->project()->errorModel()->isEmpty());

    cwCave* loadedTrips = childNamed(loadedRegion->rootNode(), QStringLiteral("Trips"));
    REQUIRE(loadedTrips != nullptr);
    CHECK(tripNamed(loadedTrips, QStringLiteral("notes")) != nullptr);

    cwCave* loadedNotes = childNamed(loadedTrips, QStringLiteral("Notes"));
    REQUIRE(loadedNotes != nullptr);
    CHECK(tripNamed(loadedNotes, QStringLiteral("external-centerline")) != nullptr);
}

TEST_CASE("cwSaveLoad repairs duplicate ids and names in a copied nested node", "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    buildTree(rootData.get(), false);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("nested-duplicate"));

    //The by-hand duplicate the depth-one repair already covers, made one level
    //deeper: every id below the copied directory arrives twice.
    const QDir dataRoot = rootData->project()->dataRootDir();
    const QDir nestedSource(dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/nodes/Side Cave")));
    REQUIRE(nestedSource.exists());
    copyDirectory(nestedSource, QDir(dataRoot.absoluteFilePath(QStringLiteral("Kentucky field seasons/nodes/Side Cave copy"))));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    auto loadedRegion = loadedRoot->project()->cavingRegion();
    cwCave* loadedFolder = childNamed(loadedRegion->rootNode(), kFolderName);
    REQUIRE(loadedFolder != nullptr);
    REQUIRE(loadedFolder->childNodeCount() == 2);

    //Every node of the whole tree carries its own id, and the copy's name was
    //deduplicated with a warning rather than left to collide on disk.
    QSet<QUuid> nodeIds;
    const QList<cwSurveyNode*> nodes = loadedRegion->rootNode()->allNodes();
    for (cwSurveyNode* node : nodes) {
        auto* cave = qobject_cast<cwCave*>(node);
        REQUIRE(cave != nullptr);
        INFO("Node " << cave->name().toStdString());
        CHECK_FALSE(nodeIds.contains(cave->id()));
        nodeIds.insert(cave->id());
    }

    QSet<QUuid> tripIds;
    const QList<cwTrip*> trips = loadedRegion->rootNode()->allTrips();
    for (cwTrip* trip : trips) {
        INFO("Trip " << trip->name().toStdString());
        CHECK_FALSE(tripIds.contains(trip->id()));
        tripIds.insert(trip->id());
    }

    CHECK(loadedFolder->childNode(0)->name() != loadedFolder->childNode(1)->name());

    int renameWarnings = 0;
    const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
    for (const cwError& error : errors) {
        if (error.message().contains(QStringLiteral("renamed to"))) {
            renameWarnings++;
        }
    }
    CHECK(renameWarnings > 0);
}

TEST_CASE("cwSaveLoad restamps a flat project when its first node appears", "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* cave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kFisherRidgeName);
    addTrip(cave, kEntranceTripName, QStringLiteral("A"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("restamp"));
    const QDir projectRootDir = QFileInfo(projectFile).absoluteDir();

    checkEveryFileVersion(projectRootDir, kFlatVersion);

    //The project's first hierarchy needs the tree format, and every file of a
    //project carries the same FileVersion: the files written before the node
    //existed are rewritten too, rather than left behind at 9.
    addNode(region, cave, cwSurveyNode::Kind::Folder, kSectionName);
    rootData->project()->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();
    rootData->project()->waitSaveToFinish();

    checkEveryFileVersion(projectRootDir, cwRegionIOTask::protoVersion());
}

TEST_CASE("cavewhereLfsPolicy classifies a note image the same at any depth", "[cwSaveLoad][NodeTree]")
{
    const QQuickGit::LfsPolicy policy = cavewhereLfsPolicy();

    const QString depthOne =
            QStringLiteral("Fisher Ridge/trips/Entrance survey/notes/Note.png");
    const QString depthThree =
            QStringLiteral("Kentucky field seasons/nodes/Side Cave/nodes/Upper level/trips/Dome climb/notes/Note.png");

    CHECK(policy.isEligible(depthOne));
    CHECK(policy.isEligible(depthThree) == policy.isEligible(depthOne));

    const QString descriptorAtDepthThree =
            QStringLiteral("Kentucky field seasons/nodes/Side Cave/nodes/Upper level/Upper level.cwcave");
    CHECK_FALSE(policy.isEligible(descriptorAtDepthThree));
}

TEST_CASE("The survey-tree depth2 fixture loads", "[cwSaveLoad][NodeTree]")
{
    const QString fixturePath = testcasesDatasetSourcePath(QStringLiteral("survey-tree/depth2"));

    if (!QDir(fixturePath).exists()) {
        //The fixture is generated once, from the same tree every other test in
        //this file builds, and checked in; from then on this test is a load
        //compatibility check against those bytes.
        auto rootData = std::make_unique<cwRootData>();
        buildTree(rootData.get(), true);

        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("depth2"));

        //The fixture is checked into CaveWhere's own repository, so it carries
        //the project's files without the git repository the save made for it.
        const QDir savedRoot = QFileInfo(projectFile).absoluteDir();
        copyDirectory(savedRoot, QDir(fixturePath));
        WARN("Generated the survey-tree/depth2 fixture at " << fixturePath.toStdString());
    }

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir workingRoot = QDir(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("depth2")));
    copyDirectory(QDir(fixturePath), workingRoot);

    const QString projectFile = projectFileIn(workingRoot);
    checkEveryFileVersion(workingRoot, cwRegionIOTask::protoVersion());

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    auto region = loadedRoot->project()->cavingRegion();
    REQUIRE(region->caveCount() == 3);

    cwCave* fisherRidge = childNamed(region->rootNode(), kFisherRidgeName);
    REQUIRE(fisherRidge != nullptr);
    CHECK(fisherRidge->tripCount() == 2);
    cwTrip* entranceTrip = tripNamed(fisherRidge, kEntranceTripName);
    REQUIRE(entranceTrip != nullptr);
    CHECK(entranceTrip->notes()->rowCount() == 1);

    cwCave* folder = childNamed(region->rootNode(), kFolderName);
    REQUIRE(folder != nullptr);
    CHECK(folder->kind() == cwSurveyNode::Kind::Folder);

    cwCave* sideCave = childNamed(folder, kSideCaveName);
    REQUIRE(sideCave != nullptr);
    CHECK(sideCave->kind() == cwSurveyNode::Kind::Cave);
    CHECK(sideCave->tripCount() == 1);

    cwCave* section = childNamed(sideCave, kSectionName);
    REQUIRE(section != nullptr);
    CHECK(section->kind() == cwSurveyNode::Kind::Folder);
    REQUIRE(section->tripCount() == 1);
    CHECK(section->trip(0)->name() == kSectionTripName);

    CHECK(childNamed(region->rootNode(), kSideCaveName) != nullptr);
    CHECK(region->equates()->count() == 1);
}

TEST_CASE("cwSaveLoad restamps every file when a move flattens the project",
          "[cwSaveLoad][NodeTree][MoveNode]")
{
    //A move reaches the tree model as a row removal and then a row insertion,
    //and halfway through it the moved subtree is off the tree. Answering the
    //version question there records a stamp the subtree's own files never get,
    //leaving a project whose descriptor says 9 while its trips and notes say 10.
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* parent = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kFisherRidgeName);
    addTrip(parent, kEntranceTripName, QStringLiteral("A"));

    cwCave* child = addNode(region, parent, cwSurveyNode::Kind::Cave, kSideCaveName);
    cwTrip* childTrip = addTrip(child, kSideCaveTripName, QStringLiteral("C"));
    const QString noteImagePath =
            copyToTempFolder(testcasesDatasetPath("test_cwAddImageTask/supportedImage.png"));
    childTrip->notes()->addFromFiles({QUrl::fromLocalFile(noteImagePath)});
    rootData->futureManagerModel()->waitForFinished();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("flatten-move"));
    const QDir projectRootDir = QFileInfo(projectFile).absoluteDir();
    checkEveryFileVersion(projectRootDir, cwRegionIOTask::protoVersion());

    //With the only nested node back at the root the project is flat again, so
    //every file of it carries the stamp an older build can still open.
    region->moveNode(child, nullptr, 0);
    flushSaves(rootData.get());

    checkEveryFileVersion(projectRootDir, kFlatVersion);
}

namespace {

const QString kChildNodeName = QStringLiteral("kid");
const QString kChildTripName = QStringLiteral("Kid survey");

//! Saves a parent named \a parentName holding one child node with one trip,
//! reloads it, and checks the whole subtree came back.
void checkNestedChildSurvivesReload(const QString& parentName)
{
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* parent = addNode(region, nullptr, cwSurveyNode::Kind::Cave, parentName);
    cwCave* child = addNode(region, parent, cwSurveyNode::Kind::Folder, kChildNodeName);
    addTrip(child, kChildTripName, QStringLiteral("K"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("sort-order"));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    INFO("Parent node \"" << parentName.toStdString() << "\"");
    const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
    for (const cwError& error : errors) {
        INFO("Error: " << error.message().toStdString());
    }
    CHECK(loadedRoot->project()->errorModel()->isEmpty());

    cwCave* loadedParent = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), parentName);
    REQUIRE(loadedParent != nullptr);
    cwCave* loadedChild = childNamed(loadedParent, kChildNodeName);
    REQUIRE(loadedChild != nullptr);
    CHECK(loadedChild->tripCount() == 1);
    CHECK(loadedParent->tripCount() == 0);
}

}

TEST_CASE("cwSaveLoad loads a nested child whose parent sorts after its nodes directory",
          "[cwSaveLoad][NodeTree]")
{
    //The loader places a parent before its children by sorting descriptors on
    //their absolute path, which holds only while the parent's own file name
    //sorts before "nodes". Every name whose first character sorts after "n"
    //breaks that, and the child's whole subtree is then re-parented on open.
    SECTION("a parent whose name starts after n")
    {
        checkNestedChildSurvivesReload(QStringLiteral("zeta"));
    }

    SECTION("a lowercase parent in the middle of the alphabet")
    {
        checkNestedChildSurvivesReload(QStringLiteral("upper level"));
    }

    SECTION("a parent named like the layout directory but longer")
    {
        checkNestedChildSurvivesReload(QStringLiteral("notes"));
    }

    SECTION("a parent whose name starts with a non-ASCII letter")
    {
        checkNestedChildSurvivesReload(QString::fromUtf8("\xC3\x81rea"));
    }
}

TEST_CASE("cwSaveLoad leaves a symlinked directory out of the project scan",
          "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    buildTree(rootData.get(), false);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("symlink-scan"));

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QString fisherRidgeTripsDir =
            dataRoot.absoluteFilePath(kFisherRidgeName + QStringLiteral("/trips"));
    const QStringList fisherRidgeTripFilesBefore = relativeFiles(QDir(fisherRidgeTripsDir));

    SECTION("a link to another project's data root")
    {
        //A second, foreign project: its descriptors name nodes this project
        //never heard of, and its trips would be adopted by whatever node the
        //link hangs under.
        auto foreignRoot = std::make_unique<cwRootData>();
        auto* foreignRegion = foreignRoot->project()->cavingRegion();
        cwCave* foreignCave = addNode(foreignRegion, nullptr, cwSurveyNode::Kind::Cave,
                                      QStringLiteral("Foreign Cave"));
        addTrip(foreignCave, QStringLiteral("Foreign survey"), QStringLiteral("F"));
        addTrip(foreignCave, QStringLiteral("Foreign second"), QStringLiteral("G"));

        QTemporaryDir foreignDir;
        REQUIRE(foreignDir.isValid());
        saveProjectAs(foreignRoot.get(), QDir(foreignDir.path()), QStringLiteral("foreign"));
        const QDir foreignDataRoot = foreignRoot->project()->dataRootDir();

        REQUIRE(QFile::link(foreignDataRoot.absolutePath(),
                            dataRoot.absoluteFilePath(kFisherRidgeName + QStringLiteral("/alias"))));

        auto loadedRoot = std::make_unique<cwRootData>();
        addTokenManager(loadedRoot->project());
        loadedRoot->project()->loadOrConvert(projectFile);
        loadedRoot->project()->waitLoadToFinish();

        const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
        for (const cwError& error : errors) {
            INFO("Error: " << error.message().toStdString());
        }
        CHECK(loadedRoot->project()->errorModel()->isEmpty());

        auto loadedRegion = loadedRoot->project()->cavingRegion();
        CHECK(loadedRegion->caveCount() == 3);
        cwCave* loadedFisherRidge = childNamed(loadedRegion->rootNode(), kFisherRidgeName);
        REQUIRE(loadedFisherRidge != nullptr);
        CHECK(loadedFisherRidge->tripCount() == 2);

        flushSaves(loadedRoot.get());
        CHECK(relativeFiles(QDir(fisherRidgeTripsDir)) == fisherRidgeTripFilesBefore);
    }

    SECTION("a link back to the project's own data root")
    {
        REQUIRE(QFile::link(dataRoot.absolutePath(),
                            dataRoot.absoluteFilePath(kFisherRidgeName + QStringLiteral("/loop"))));

        auto loadedRoot = std::make_unique<cwRootData>();
        addTokenManager(loadedRoot->project());
        loadedRoot->project()->loadOrConvert(projectFile);
        loadedRoot->project()->waitLoadToFinish();

        CHECK(loadedRoot->project()->errorModel()->isEmpty());
        CHECK(loadedRoot->project()->cavingRegion()->caveCount() == 3);
    }
}

TEST_CASE("cwSaveLoad renames a loaded node at depth two and depth three",
          "[cwSaveLoad][NodeTree]")
{
    static const QString kRenamedCave = QStringLiteral("Renamed cave");
    static const QString kRenamedSection = QStringLiteral("Renamed section");

    const QString fixturePath = testcasesDatasetSourcePath(QStringLiteral("survey-tree/depth2"));
    REQUIRE(QDir(fixturePath).exists());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir workingRoot(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("depth2")));
    copyDirectory(QDir(fixturePath), workingRoot);

    const QString projectFile = projectFileIn(workingRoot);

    auto rootData = std::make_unique<cwRootData>();
    addTokenManager(rootData->project());
    rootData->project()->loadOrConvert(projectFile);
    rootData->project()->waitLoadToFinish();
    REQUIRE(rootData->project()->errorModel()->isEmpty());

    auto region = rootData->project()->cavingRegion();
    cwCave* folder = childNamed(region->rootNode(), kFolderName);
    REQUIRE(folder != nullptr);
    cwCave* sideCave = childNamed(folder, kSideCaveName);
    REQUIRE(sideCave != nullptr);
    cwCave* section = childNamed(sideCave, kSectionName);
    REQUIRE(section != nullptr);

    const QDir dataRoot = rootData->project()->dataRootDir();
    const int fileCountBefore = relativeFiles(dataRoot).size();

    //A node loaded from disk carries the path it was read from, so a rename
    //moves that directory rather than writing a second one beside it.
    sideCave->setName(kRenamedCave);
    flushSaves(rootData.get());

    section->setName(kRenamedSection);
    flushSaves(rootData.get());

    const QString renamedCaveDir =
            QStringLiteral("%1/nodes/%2").arg(kFolderName, kRenamedCave);
    CHECK_FALSE(QFileInfo::exists(
                    dataRoot.absoluteFilePath(QStringLiteral("%1/nodes/%2").arg(kFolderName, kSideCaveName))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(
                                QStringLiteral("%1/%2.cwcave").arg(renamedCaveDir, kRenamedCave))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(
                                QStringLiteral("%1/trips/%2/%2.cwtrip").arg(renamedCaveDir, kSideCaveTripName))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(
                                QStringLiteral("%1/nodes/%2/%2.cwcave").arg(renamedCaveDir, kRenamedSection))));
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(
                                QStringLiteral("%1/nodes/%2/trips/%3/%3.cwtrip")
                                .arg(renamedCaveDir, kRenamedSection, kSectionTripName))));
    CHECK(relativeFiles(dataRoot).size() == fileCountBefore);

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();
    CHECK(loadedRoot->project()->errorModel()->isEmpty());

    cwCave* loadedFolder = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName);
    REQUIRE(loadedFolder != nullptr);
    cwCave* loadedCave = childNamed(loadedFolder, kRenamedCave);
    REQUIRE(loadedCave != nullptr);
    CHECK(childNamed(loadedCave, kRenamedSection) != nullptr);
}

TEST_CASE("cwSaveLoad rewrites nothing when it opens a nested project",
          "[cwSaveLoad][NodeTree]")
{
    const QString fixturePath = testcasesDatasetSourcePath(QStringLiteral("survey-tree/depth2"));
    REQUIRE(QDir(fixturePath).exists());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir workingRoot(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("depth2")));
    copyDirectory(QDir(fixturePath), workingRoot);

    const QString projectFile = projectFileIn(workingRoot);

    const QStringList filesBefore = relativeFiles(workingRoot);
    QList<QByteArray> contentBefore;
    for (const QString& relative : filesBefore) {
        contentBefore.append(readFile(workingRoot.absoluteFilePath(relative)));
    }

    auto rootData = std::make_unique<cwRootData>();
    addTokenManager(rootData->project());
    rootData->project()->loadOrConvert(projectFile);
    rootData->project()->waitLoadToFinish();
    flushSaves(rootData.get());

    CHECK(rootData->project()->errorModel()->isEmpty());
    REQUIRE(relativeFiles(workingRoot) == filesBefore);
    for (int i = 0; i < filesBefore.size(); i++) {
        INFO("File " << filesBefore.at(i).toStdString());
        CHECK(readFile(workingRoot.absoluteFilePath(filesBefore.at(i))) == contentBefore.at(i));
    }
}

TEST_CASE("cwSaveLoad stamps the tree version for each field that needs it",
          "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* cave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kFisherRidgeName);
    addTrip(cave, kEntranceTripName, QStringLiteral("A"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("stamp-fields"));
    const QDir projectRootDir = QFileInfo(projectFile).absoluteDir();

    checkEveryFileVersion(projectRootDir, kFlatVersion);

    SECTION("a kind an older build cannot express")
    {
        cave->setKind(cwSurveyNode::Kind::Folder);
        flushSaves(rootData.get());
        checkEveryFileVersion(projectRootDir, cwRegionIOTask::protoVersion());
    }

    SECTION("a read-only node")
    {
        cave->setReadOnly(true);
        flushSaves(rootData.get());
        checkEveryFileVersion(projectRootDir, cwRegionIOTask::protoVersion());
    }

    SECTION("a node that came from an external source")
    {
        cave->setSourceId(QUuid::createUuid());
        flushSaves(rootData.get());
        checkEveryFileVersion(projectRootDir, cwRegionIOTask::protoVersion());
    }

TEST_CASE("cwSaveLoad reports what it cannot place and loads the rest of the tree",
          "[cwSaveLoad][NodeTree]")
{
    auto rootData = std::make_unique<cwRootData>();
    buildTree(rootData.get(), false);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("load-report"));

    const QDir dataRoot = rootData->project()->dataRootDir();

    const auto loadReport = [&projectFile](std::unique_ptr<cwRootData>& loadedRoot) {
        loadedRoot = std::make_unique<cwRootData>();
        addTokenManager(loadedRoot->project());
        loadedRoot->project()->loadOrConvert(projectFile);
        loadedRoot->project()->waitLoadToFinish();

        QStringList messages;
        const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
        for (const cwError& error : errors) {
            messages.append(error.message());
        }
        return messages;
    };

    const auto countNaming = [](const QStringList& messages, const QString& needle) {
        int count = 0;
        for (const QString& message : messages) {
            if (message.contains(needle)) {
                count++;
            }
        }
        return count;
    };

    std::unique_ptr<cwRootData> loadedRoot;

    SECTION("an ancestor descriptor that cannot be parsed")
    {
        QFile folderDescriptor(dataRoot.absoluteFilePath(
                                   QStringLiteral("%1/%1.cwcave").arg(kFolderName)));
        REQUIRE(folderDescriptor.open(QIODevice::WriteOnly | QIODevice::Truncate));
        folderDescriptor.write("{garbage");
        folderDescriptor.close();

        const QStringList messages = loadReport(loadedRoot);
        INFO("Errors: " << messages.join(QStringLiteral(" | ")).toStdString());

        CHECK(countNaming(messages, kFolderName) > 0);
        CHECK(countNaming(messages, kSideCaveName) > 0);
        CHECK(countNaming(messages, kSectionName) > 0);
        CHECK(countNaming(messages, QStringLiteral("belongs to no survey node")) >= 2);

        //The siblings of the unreadable node are never dropped with it.
        auto loadedRegion = loadedRoot->project()->cavingRegion();
        CHECK(childNamed(loadedRegion->rootNode(), kFisherRidgeName) != nullptr);
        CHECK(childNamed(loadedRegion->rootNode(), kSideCaveName) != nullptr);
    }

    SECTION("a descriptor sitting directly in a nodes directory")
    {
        const QString loosePath =
                dataRoot.absoluteFilePath(QStringLiteral("%1/nodes/Loose.cwcave").arg(kFolderName));
        REQUIRE(QFile::copy(dataRoot.absoluteFilePath(QStringLiteral("Side Cave/Side Cave.cwcave")),
                            loosePath));

        const QStringList messages = loadReport(loadedRoot);
        INFO("Errors: " << messages.join(QStringLiteral(" | ")).toStdString());
        CHECK(countNaming(messages, QStringLiteral("Loose.cwcave")) == 1);
        CHECK(messages.size() == 1);

        auto loadedRegion = loadedRoot->project()->cavingRegion();
        CHECK(loadedRegion->caveCount() == 3);
        cwCave* loadedFolder = childNamed(loadedRegion->rootNode(), kFolderName);
        REQUIRE(loadedFolder != nullptr);
        CHECK(childNamed(loadedFolder, kSideCaveName) != nullptr);
    }

    SECTION("an empty nodes directory")
    {
        REQUIRE(QDir().mkpath(dataRoot.absoluteFilePath(
                                  kFisherRidgeName + QStringLiteral("/nodes"))));

        const QStringList messages = loadReport(loadedRoot);
        INFO("Errors: " << messages.join(QStringLiteral(" | ")).toStdString());
        CHECK(messages.isEmpty());
        CHECK(loadedRoot->project()->cavingRegion()->caveCount() == 3);
    }

    SECTION("a plain file named after the nodes directory")
    {
        QFile strayFile(dataRoot.absoluteFilePath(kFisherRidgeName + QStringLiteral("/nodes")));
        REQUIRE(strayFile.open(QIODevice::WriteOnly));
        strayFile.write("not a directory");
        strayFile.close();

        const QStringList messages = loadReport(loadedRoot);
        INFO("Errors: " << messages.join(QStringLiteral(" | ")).toStdString());
        CHECK(messages.isEmpty());
        CHECK(loadedRoot->project()->cavingRegion()->caveCount() == 3);
    }
}

TEST_CASE("cwSaveLoad restamps every file when a flattening move is undone",
          "[cwSaveLoad][NodeTree][MoveNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* parent = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kFisherRidgeName);
    addTrip(parent, kEntranceTripName, QStringLiteral("A"));

    cwCave* child = addNode(region, parent, cwSurveyNode::Kind::Cave, kSideCaveName);
    addTrip(child, kSideCaveTripName, QStringLiteral("C"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("flatten-undo"));
    const QDir projectRootDir = QFileInfo(projectFile).absoluteDir();
    const int fileCountBefore = relativeFiles(projectRootDir).size();

    SECTION("a move away from the parent and back")
    {
        region->moveNode(child, nullptr, 0);
        flushSaves(rootData.get());
        checkEveryFileVersion(projectRootDir, kFlatVersion);

        rootData->undoStack()->undo();
        flushSaves(rootData.get());
        checkEveryFileVersion(projectRootDir, cwRegionIOTask::protoVersion());
        CHECK(relativeFiles(projectRootDir).size() == fileCountBefore);

        rootData->undoStack()->redo();
        flushSaves(rootData.get());
        checkEveryFileVersion(projectRootDir, kFlatVersion);
        CHECK(relativeFiles(projectRootDir).size() == fileCountBefore);
    }

    SECTION("a delete that leaves the project flat")
    {
        parent->removeNode(parent->indexOfNode(child));
        flushSaves(rootData.get());
        checkEveryFileVersion(projectRootDir, kFlatVersion);
    }
}

TEST_CASE("cwSaveLoad renames the data root of a nested project",
          "[cwSaveLoad][NodeTree]")
{
    static const QString kRenamedRegion = QStringLiteral("Renamed region");

    auto rootData = std::make_unique<cwRootData>();
    buildTree(rootData.get(), false);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("region-rename"));

    const QStringList filesBefore = relativeFiles(rootData->project()->dataRootDir());

    rootData->project()->cavingRegion()->setName(kRenamedRegion);
    flushSaves(rootData.get());

    const QDir newDataRoot = rootData->project()->dataRootDir();
    CHECK(newDataRoot.dirName() == kRenamedRegion);
    CHECK(relativeFiles(newDataRoot) == filesBefore);
    CHECK(QFileInfo::exists(newDataRoot.absoluteFilePath(
                                QStringLiteral("%1/nodes/%2/nodes/%3/%3.cwcave")
                                .arg(kFolderName, kSideCaveName, kSectionName))));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(rootData->project()->filename());
    loadedRoot->project()->waitLoadToFinish();

    QStringList messages;
    const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
    for (const cwError& error : errors) {
        messages.append(error.message());
    }
    INFO("Errors: " << messages.join(QStringLiteral(" | ")).toStdString());
    CHECK(loadedRoot->project()->errorModel()->isEmpty());

    auto loadedRegion = loadedRoot->project()->cavingRegion();
    cwCave* loadedFolder = childNamed(loadedRegion->rootNode(), kFolderName);
    REQUIRE(loadedFolder != nullptr);
    cwCave* loadedSideCave = childNamed(loadedFolder, kSideCaveName);
    REQUIRE(loadedSideCave != nullptr);
    CHECK(childNamed(loadedSideCave, kSectionName) != nullptr);
}

TEST_CASE("cwSaveLoad round trips a nested tree through a bundled file",
          "[cwSaveLoad][NodeTree][bundled]")
{
    static const QString kEditedTripName = QStringLiteral("Dome climb revisited");

    auto rootData = std::make_unique<cwRootData>();
    //A bundled save commits to the project's own repository, which needs an
    //author before it will write anything.
    rootData->account()->setName(QStringLiteral("Author User"));
    rootData->account()->setEmail(QStringLiteral("author@example.com"));

    const TreeFixture fixture = buildTree(rootData.get(), true);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString bundlePath = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("tree.cw"));
    REQUIRE(rootData->project()->saveAs(bundlePath));
    flushSaves(rootData.get());
    REQUIRE(QFileInfo::exists(bundlePath));

    const auto loadBundle = [&bundlePath]() {
        auto loadedRoot = std::make_unique<cwRootData>();
        addTokenManager(loadedRoot->project());
        loadedRoot->project()->loadOrConvert(bundlePath);
        loadedRoot->project()->waitLoadToFinish();
        CHECK(loadedRoot->project()->errorModel()->isEmpty());
        return loadedRoot;
    };

    {
        auto loadedRoot = loadBundle();
        auto loadedRegion = loadedRoot->project()->cavingRegion();
        cwCave* loadedFolder = childNamed(loadedRegion->rootNode(), kFolderName);
        REQUIRE(loadedFolder != nullptr);
        CHECK(loadedFolder->kind() == cwSurveyNode::Kind::Folder);
        cwCave* loadedSideCave = childNamed(loadedFolder, kSideCaveName);
        REQUIRE(loadedSideCave != nullptr);
        cwCave* loadedSection = childNamed(loadedSideCave, kSectionName);
        REQUIRE(loadedSection != nullptr);
        CHECK(loadedSection->kind() == cwSurveyNode::Kind::Folder);
        REQUIRE(loadedSection->tripCount() == 1);
        CHECK(loadedSection->trip(0)->name() == kSectionTripName);
        CHECK(childNamed(loadedRegion->rootNode(), kSideCaveName) != nullptr);
    }

    //An edit three levels down and a plain save: the bundle is rewritten from
    //the same tree, not from a flattened copy of it.
    REQUIRE(fixture.section->tripCount() == 1);
    fixture.section->trip(0)->setName(kEditedTripName);
    flushSaves(rootData.get());
    //A bundle is rewritten by an explicit save, not by the working directory's
    //own queue draining.
    REQUIRE(rootData->project()->save());
    flushSaves(rootData.get());

    auto loadedRoot = loadBundle();
    cwCave* loadedFolder = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName);
    REQUIRE(loadedFolder != nullptr);
    cwCave* loadedSideCave = childNamed(loadedFolder, kSideCaveName);
    REQUIRE(loadedSideCave != nullptr);
    cwCave* loadedSection = childNamed(loadedSideCave, kSectionName);
    REQUIRE(loadedSection != nullptr);
    CHECK(tripNamed(loadedSection, kEditedTripName) != nullptr);
}

TEST_CASE("cwSaveLoad renames a nested node in a Save As copy and leaves the source alone",
          "[cwSaveLoad][NodeTree][saveAs]")
{
    static const QString kRenamedCave = QStringLiteral("Copied side cave");

    auto rootData = std::make_unique<cwRootData>();
    const TreeFixture fixture = buildTree(rootData.get(), false);

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString sourceProjectFile =
            saveProjectAs(rootData.get(), QDir(sourceDir.path()), QStringLiteral("save-as-source"));
    const QDir sourceDataRoot = rootData->project()->dataRootDir();
    const QStringList sourceFilesBefore = relativeFiles(sourceDataRoot);

    QTemporaryDir copyDir;
    REQUIRE(copyDir.isValid());
    saveProjectAs(rootData.get(), QDir(copyDir.path()), QStringLiteral("save-as-copy"));
    const QDir copyDataRoot = rootData->project()->dataRootDir();
    REQUIRE(copyDataRoot.absolutePath() != sourceDataRoot.absolutePath());

    //The copy is the live project now, so a rename inside it moves the copy's
    //directory and never reaches the project it was copied from.
    fixture.sideCave->setName(kRenamedCave);
    flushSaves(rootData.get());

    CHECK(QFileInfo::exists(copyDataRoot.absoluteFilePath(
                                QStringLiteral("%1/nodes/%2/%2.cwcave").arg(kFolderName, kRenamedCave))));
    CHECK_FALSE(QFileInfo::exists(copyDataRoot.absoluteFilePath(
                                      QStringLiteral("%1/nodes/%2").arg(kFolderName, kSideCaveName))));
    CHECK(relativeFiles(sourceDataRoot) == sourceFilesBefore);

    auto loadedSource = std::make_unique<cwRootData>();
    addTokenManager(loadedSource->project());
    loadedSource->project()->loadOrConvert(sourceProjectFile);
    loadedSource->project()->waitLoadToFinish();
    CHECK(loadedSource->project()->errorModel()->isEmpty());

    cwCave* sourceFolder = childNamed(loadedSource->project()->cavingRegion()->rootNode(), kFolderName);
    REQUIRE(sourceFolder != nullptr);
    CHECK(childNamed(sourceFolder, kSideCaveName) != nullptr);

    auto loadedCopy = std::make_unique<cwRootData>();
    addTokenManager(loadedCopy->project());
    loadedCopy->project()->loadOrConvert(rootData->project()->filename());
    loadedCopy->project()->waitLoadToFinish();
    CHECK(loadedCopy->project()->errorModel()->isEmpty());

    cwCave* copyFolder = childNamed(loadedCopy->project()->cavingRegion()->rootNode(), kFolderName);
    REQUIRE(copyFolder != nullptr);
    CHECK(childNamed(copyFolder, kRenamedCave) != nullptr);
}
