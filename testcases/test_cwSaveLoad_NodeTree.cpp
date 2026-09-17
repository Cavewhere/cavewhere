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
#include <QUrl>
#include <QUuid>

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

constexpr double kShotDistance = 10.0;
constexpr double kShotCompass = 45.0;
constexpr double kShotClino = -5.0;

//! Gives \a trip one shot, so it holds real survey data on disk.
void addShot(cwTrip* trip, const QString& fromStation, const QString& toStation)
{
    trip->addNewChunk();
    cwSurveyChunk* chunk = trip->chunk(trip->chunkCount() - 1);
    chunk->setData(cwSurveyChunk::StationNameRole, 0, fromStation);
    chunk->setData(cwSurveyChunk::StationNameRole, 1, toStation);
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, kShotDistance);
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, kShotCompass);
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, kShotClino);
}

cwCave* addNode(cwCavingRegion* region,
                cwSurveyNode* parent,
                cwSurveyNode::Kind kind,
                const QString& name)
{
    auto node = qobject_cast<cwCave*>(region->addNode(parent, kind));
    REQUIRE(node != nullptr);
    node->setName(name);
    return node;
}

cwTrip* addTrip(cwCave* node, const QString& name, const QString& station)
{
    node->addTrip();
    cwTrip* trip = node->trip(node->tripCount() - 1);
    REQUIRE(trip != nullptr);
    trip->setName(name);
    addShot(trip, station + QStringLiteral("1"), station + QStringLiteral("2"));
    return trip;
}

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

QString saveProjectAs(cwRootData* rootData, const QDir& parentDir, const QString& baseName)
{
    auto project = rootData->project();
    const QString projectPath = parentDir.absoluteFilePath(baseName + QStringLiteral(".cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    return project->filename();
}

//! Every file below \a dir, as data-root-relative paths, with git's own files
//! left out — they are not part of the project's layout.
QStringList relativeFiles(const QDir& dir)
{
    QStringList files;
    QDirIterator it(dir.absolutePath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = dir.relativeFilePath(path);
        if (relative.startsWith(QStringLiteral(".git"))) {
            continue;
        }
        files.append(relative);
    }
    files.sort();
    return files;
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
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
