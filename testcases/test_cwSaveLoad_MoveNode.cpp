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
#include "cwErrorListModel.h"
#include "cwFutureManagerModel.h"
#include "cwNote.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "LoadProjectHelper.h"
#include "ProjectFilenameTestHelper.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"

//Qt includes
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUrl>

using namespace SurveyTreeTestHelper;

namespace {

const QString kFolderName = QStringLiteral("Kentucky field seasons");
const QString kCaveName = QStringLiteral("Side Cave");
const QString kTripName = QStringLiteral("Sump dig");
const QString kSectionName = QStringLiteral("Upper level");
const QString kSectionTripName = QStringLiteral("Dome climb");
const QString kAttachmentName = QStringLiteral("entry.svx");
const QString kAttachmentContent = QStringLiteral("*begin entry\n*end entry\n");

void writeFile(const QString& path, const QByteArray& content)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(content);
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

cwTrip* tripNamed(const cwCave* node, const QString& name)
{
    for (int i = 0; i < node->tripCount(); i++) {
        if (node->trip(i)->name() == name) {
            return node->trip(i);
        }
    }
    return nullptr;
}

//! Every node's path, joined, as the tree reads it after a reload — the whole
//! shape in one comparable list.
QStringList nodePaths(const cwCavingRegion* region)
{
    QStringList paths;
    region->rootNode()->walk([&paths](const cwSurveyNode* node) {
        if (node->isRoot()) {
            return;
        }
        QStringList entry = node->path();
        const QList<cwTrip*> trips = node->trips();
        for (const cwTrip* trip : trips) {
            entry.append(QStringLiteral("[%1]").arg(trip->name()));
        }
        paths.append(entry.join(QStringLiteral("/")));
    });
    paths.sort();
    return paths;
}

//! Loads \a projectFile in its own root data and hands the caller the tree it
//! read back, so a test can compare disk against memory after every step.
QStringList reloadedNodePaths(const QString& projectFile)
{
    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();
    CHECK(loadedRoot->project()->errorModel()->isEmpty());
    return nodePaths(loadedRoot->project()->cavingRegion());
}

struct MoveFixture {
    cwCave* folder = nullptr;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;
};

//! Folder "Kentucky field seasons" and Cave "Side Cave" side by side at the top
//! level, the cave holding one trip with a real note image and a cave-level
//! external-centerline attachment — so a move has a whole subtree of files to
//! carry, not only a descriptor.
MoveFixture buildFixture(cwRootData* rootData)
{
    auto region = rootData->project()->cavingRegion();

    MoveFixture fixture;
    fixture.folder = addNode(region, nullptr, cwSurveyNode::Kind::Folder, kFolderName);
    fixture.cave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kCaveName);
    fixture.trip = addTrip(fixture.cave, kTripName, QStringLiteral("A"));

    const QString noteImagePath =
            copyToTempFolder(testcasesDatasetPath("test_cwAddImageTask/supportedImage.png"));
    fixture.trip->notes()->addFromFiles({QUrl::fromLocalFile(noteImagePath)});
    rootData->futureManagerModel()->waitForFinished();

    return fixture;
}

}

TEST_CASE("cwSaveLoad moves a node's directory rather than deleting it",
          "[cwSaveLoad][MoveNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    const MoveFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("move-node"));

    //A cave-level attachment, planted the way a copied source file lands there.
    const QDir attachmentDir = ProjectFilenameTestHelper::externalCenterlineDir(fixture.cave);
    writeFile(attachmentDir.absoluteFilePath(kAttachmentName), kAttachmentContent.toUtf8());

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QDir oldCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    REQUIRE(oldCaveDir.absolutePath() == dataRoot.absoluteFilePath(kCaveName));

    const QStringList filesBefore = relativeFiles(oldCaveDir);
    REQUIRE(filesBefore.contains(QStringLiteral("external-centerline/") + kAttachmentName));
    REQUIRE(filesBefore.contains(kCaveName + QStringLiteral(".cwcave")));

    const QString noteImageName = fixture.trip->notes()->notes().first()->image().path();
    const QString oldNoteImagePath =
            ProjectFilenameTestHelper::absolutePath(fixture.trip->notes()->notes().first(), noteImageName);
    REQUIRE(QFileInfo::exists(oldNoteImagePath));
    const QByteArray noteImageBytes = readFile(oldNoteImagePath);

    const QStringList pathsBefore = nodePaths(rootData->project()->cavingRegion());

    rootData->project()->cavingRegion()->moveNode(fixture.cave, fixture.folder, 0);
    flushSaves(rootData.get());

    const QDir newCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    CHECK(newCaveDir.absolutePath()
          == dataRoot.absoluteFilePath(kFolderName + QStringLiteral("/nodes/") + kCaveName));

    //The whole directory traveled: descriptor, trip, note image and attachment,
    //byte for byte, and nothing was left at the old path.
    CHECK(relativeFiles(newCaveDir) == filesBefore);
    CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));
    CHECK(readFile(ProjectFilenameTestHelper::absolutePath(
                       fixture.trip->notes()->notes().first(), noteImageName)) == noteImageBytes);
    CHECK(relativeFiles(dataRoot).filter(kCaveName + QStringLiteral("/")).size()
          == filesBefore.size());

    CHECK(reloadedNodePaths(projectFile) == nodePaths(rootData->project()->cavingRegion()));

    SECTION("undo carries the directory back, and redo takes it away again")
    {
        rootData->undoStack()->undo();
        flushSaves(rootData.get());

        CHECK(ProjectFilenameTestHelper::dir(fixture.cave).absolutePath()
              == oldCaveDir.absolutePath());
        CHECK(relativeFiles(oldCaveDir) == filesBefore);
        CHECK(readFile(ProjectFilenameTestHelper::absolutePath(
                           fixture.trip->notes()->notes().first(), noteImageName)) == noteImageBytes);

        //The folder kept its own descriptor and holds nothing else.
        const QDir folderDir = ProjectFilenameTestHelper::dir(fixture.folder);
        CHECK(relativeFiles(folderDir)
              == QStringList({kFolderName + QStringLiteral(".cwcave")}));

        CHECK(nodePaths(rootData->project()->cavingRegion()) == pathsBefore);
        CHECK(reloadedNodePaths(projectFile) == pathsBefore);

        rootData->undoStack()->redo();
        flushSaves(rootData.get());

        CHECK(relativeFiles(newCaveDir) == filesBefore);
        CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));
        CHECK(reloadedNodePaths(projectFile) == nodePaths(rootData->project()->cavingRegion()));
    }

    SECTION("a name the new siblings already use is deduplicated on disk")
    {
        rootData->undoStack()->undo();
        flushSaves(rootData.get());

        //A second node of the same name under the folder, so the move has to
        //rename what it carries.
        cwCave* twin = addNode(rootData->project()->cavingRegion(),
                               fixture.folder, cwSurveyNode::Kind::Cave, kCaveName);
        flushSaves(rootData.get());

        rootData->project()->cavingRegion()->moveNode(fixture.cave, fixture.folder, 1);
        flushSaves(rootData.get());

        CHECK(fixture.cave->name() != kCaveName);
        const QDir dedupedDir = ProjectFilenameTestHelper::dir(fixture.cave);
        CHECK(dedupedDir.dirName() == fixture.cave->name());
        //The same files, with the descriptor renamed to match the node — one
        //directory move plus one file rename, and no second descriptor left
        //under the old name.
        QStringList expectedFiles = filesBefore;
        expectedFiles.replaceInStrings(kCaveName + QStringLiteral(".cwcave"),
                                       fixture.cave->name() + QStringLiteral(".cwcave"));
        expectedFiles.sort();
        CHECK(relativeFiles(dedupedDir) == expectedFiles);
        CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(twin)));
        CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));
        CHECK(reloadedNodePaths(projectFile) == nodePaths(rootData->project()->cavingRegion()));
    }

    SECTION("a write enqueued before the move lands at the new path")
    {
        //Both the rename and the move reach the queue before it drains, so the
        //trip's file has to end up where the move put its directory.
        const QString newTripName = QStringLiteral("Sump dig 2");
        fixture.trip->setName(newTripName);
        rootData->project()->cavingRegion()->moveNode(fixture.cave, fixture.folder, 0);
        flushSaves(rootData.get());

        const QDir deeperCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
        CHECK(QFileInfo::exists(deeperCaveDir.absoluteFilePath(
                                    QStringLiteral("trips/%1/%1.cwtrip").arg(newTripName))));
        CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));
        CHECK(reloadedNodePaths(projectFile) == nodePaths(rootData->project()->cavingRegion()));
    }
}

TEST_CASE("cwSaveLoad moves a depth-2 node to the root and back",
          "[cwSaveLoad][MoveNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* folder = addNode(region, nullptr, cwSurveyNode::Kind::Folder, kFolderName);
    cwCave* cave = addNode(region, folder, cwSurveyNode::Kind::Cave, kCaveName);
    addTrip(cave, kTripName, QStringLiteral("A"));
    cwCave* section = addNode(region, cave, cwSurveyNode::Kind::Folder, kSectionName);
    addTrip(section, kSectionTripName, QStringLiteral("B"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("depth2-move"));

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QStringList sectionFiles = relativeFiles(ProjectFilenameTestHelper::dir(section));
    REQUIRE(sectionFiles.size() == 2);

    const QStringList pathsBefore = nodePaths(region);

    region->moveNode(section, nullptr, 0);
    flushSaves(rootData.get());

    CHECK(ProjectFilenameTestHelper::dir(section).absolutePath()
          == dataRoot.absoluteFilePath(kSectionName));
    CHECK(relativeFiles(QDir(dataRoot.absoluteFilePath(kSectionName))) == sectionFiles);
    CHECK_FALSE(QFileInfo::exists(
                    dataRoot.absoluteFilePath(QStringLiteral("%1/nodes/%2/nodes")
                                              .arg(kFolderName, kCaveName))
                    + QStringLiteral("/") + kSectionName));
    CHECK(region->caveCount() == 2);
    CHECK(reloadedNodePaths(projectFile) == nodePaths(region));

    rootData->undoStack()->undo();
    flushSaves(rootData.get());

    CHECK(ProjectFilenameTestHelper::dir(section).absolutePath()
          == dataRoot.absoluteFilePath(QStringLiteral("%1/nodes/%2/nodes/%3")
                                       .arg(kFolderName, kCaveName, kSectionName)));
    CHECK(relativeFiles(ProjectFilenameTestHelper::dir(section)) == sectionFiles);
    CHECK_FALSE(QFileInfo::exists(dataRoot.absoluteFilePath(kSectionName)));
    CHECK(nodePaths(region) == pathsBefore);
    CHECK(reloadedNodePaths(projectFile) == pathsBefore);

    //The tree the reload built is the tree the move left behind, trips included.
    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    cwCave* loadedFolder = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName);
    REQUIRE(loadedFolder != nullptr);
    cwCave* loadedCave = childNamed(loadedFolder, kCaveName);
    REQUIRE(loadedCave != nullptr);
    cwCave* loadedSection = childNamed(loadedCave, kSectionName);
    REQUIRE(loadedSection != nullptr);
    CHECK(tripNamed(loadedSection, kSectionTripName) != nullptr);
}

TEST_CASE("cwSaveLoad moves a node twice before the queue drains",
          "[cwSaveLoad][MoveNode]")
{
    //The first move deduplicates the node's name, so it queues a directory move
    //and the rename of the descriptor inside it; the second move queues another
    //directory move. Whether those three run one at a time or arrive as one
    //batch, the node ends up in one directory holding one descriptor named
    //after it. (The batch's compression is pinned directly in
    //test_cwSaveLoadJobCompression.cpp, where the order is deterministic.)
    auto rootData = std::make_unique<cwRootData>();
    const MoveFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("move-node-twice"));

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QDir oldCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    const QStringList filesBefore = relativeFiles(oldCaveDir);

    auto region = rootData->project()->cavingRegion();

    //A twin under the folder, so the move into it has to rename what it carries.
    cwCave* twin = addNode(region, fixture.folder, cwSurveyNode::Kind::Cave, kCaveName);
    flushSaves(rootData.get());

    region->moveNode(fixture.cave, fixture.folder, 1);
    region->moveNode(fixture.cave, nullptr, 0);
    flushSaves(rootData.get());

    const QString dedupedName = fixture.cave->name();
    CHECK(dedupedName != kCaveName);

    const QDir finalDir = ProjectFilenameTestHelper::dir(fixture.cave);
    CHECK(finalDir.absolutePath() == dataRoot.absoluteFilePath(dedupedName));

    //One descriptor, named after the node, beside the trip, note image and
    //attachment the directory carried.
    QStringList expectedFiles = filesBefore;
    expectedFiles.replaceInStrings(kCaveName + QStringLiteral(".cwcave"),
                                   dedupedName + QStringLiteral(".cwcave"));
    expectedFiles.sort();
    CHECK(relativeFiles(finalDir) == expectedFiles);
    CHECK_FALSE(QFileInfo::exists(finalDir.absoluteFilePath(kCaveName + QStringLiteral(".cwcave"))));

    //Nothing stayed behind, and the folder never kept a directory for a node
    //that only passed through it.
    CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));
    CHECK_FALSE(QFileInfo::exists(
                    dataRoot.absoluteFilePath(QStringLiteral("%1/nodes/%2").arg(kFolderName, dedupedName))));
    CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(twin)));

    CHECK(reloadedNodePaths(projectFile) == nodePaths(region));
}
