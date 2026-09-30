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
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
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

//AsyncFuture includes
#include <asyncfuture.h>

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

//Plenty of headroom for the attach pipeline's scan and save queue under a
//loaded machine; the attach itself finishes in milliseconds.
constexpr int kAttachTimeoutMs = 10000;

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
          == dataRoot.absoluteFilePath(kFolderName + QStringLiteral("/sub/") + kCaveName));

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
                    dataRoot.absoluteFilePath(QStringLiteral("%1/sub/%2/sub")
                                              .arg(kFolderName, kCaveName))
                    + QStringLiteral("/") + kSectionName));
    CHECK(region->caveCount() == 2);
    CHECK(reloadedNodePaths(projectFile) == nodePaths(region));

    rootData->undoStack()->undo();
    flushSaves(rootData.get());

    CHECK(ProjectFilenameTestHelper::dir(section).absolutePath()
          == dataRoot.absoluteFilePath(QStringLiteral("%1/sub/%2/sub/%3")
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
                    dataRoot.absoluteFilePath(QStringLiteral("%1/sub/%2").arg(kFolderName, dedupedName))));
    CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(twin)));

    CHECK(reloadedNodePaths(projectFile) == nodePaths(region));
}

TEST_CASE("cwSaveLoad orders a rename and a move queued in one turn",
          "[cwSaveLoad][MoveNode]")
{
    static const QString kRenamedCave = QStringLiteral("Renamed cave");

    auto rootData = std::make_unique<cwRootData>();
    const MoveFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("rename-and-move"));

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QDir oldCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    const QStringList filesBefore = relativeFiles(oldCaveDir);

    auto region = rootData->project()->cavingRegion();

    //A twin of the moving node's name under the folder, so a move that keeps
    //the name has to deduplicate it.
    cwCave* twin = addNode(region, fixture.folder, cwSurveyNode::Kind::Cave, kCaveName);
    flushSaves(rootData.get());
    const QStringList twinFiles = relativeFiles(ProjectFilenameTestHelper::dir(twin));

    const auto checkOneDescriptorNamed = [&](const QString& name) {
        const QDir finalDir = ProjectFilenameTestHelper::dir(fixture.cave);
        CHECK(fixture.cave->name() == name);
        CHECK(finalDir.absolutePath()
              == dataRoot.absoluteFilePath(QStringLiteral("%1/sub/%2").arg(kFolderName, name)));

        QStringList expectedFiles = filesBefore;
        expectedFiles.replaceInStrings(kCaveName + QStringLiteral(".cwcave"),
                                       name + QStringLiteral(".cwcave"));
        expectedFiles.sort();
        CHECK(relativeFiles(finalDir) == expectedFiles);

        CHECK(relativeFiles(dataRoot).filter(QStringLiteral("Side Cave 2.cwcave")).isEmpty());
        CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));
        CHECK(relativeFiles(ProjectFilenameTestHelper::dir(twin)) == twinFiles);
        CHECK(reloadedNodePaths(projectFile) == nodePaths(region));
    };

    SECTION("the rename is queued first")
    {
        fixture.cave->setName(kRenamedCave);
        region->moveNode(fixture.cave, fixture.folder, 1);
        flushSaves(rootData.get());

        checkOneDescriptorNamed(kRenamedCave);
    }

    SECTION("the move is queued first and deduplicates the name")
    {
        region->moveNode(fixture.cave, fixture.folder, 1);
        REQUIRE(fixture.cave->name() != kCaveName);
        fixture.cave->setName(kRenamedCave);
        flushSaves(rootData.get());

        checkOneDescriptorNamed(kRenamedCave);
    }
}

TEST_CASE("cwSaveLoad carries a pending chunk write through a move",
          "[cwSaveLoad][MoveNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    const MoveFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("write-then-move"));

    const QDir oldCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);

    //Real survey data, not a rename: the trip's own file has to be written, and
    //the move decides where it lands.
    addShot(fixture.trip, QStringLiteral("A2"), QStringLiteral("A3"));
    const int chunkCount = fixture.trip->chunkCount();
    REQUIRE(chunkCount == 2);

    rootData->project()->cavingRegion()->moveNode(fixture.cave, fixture.folder, 0);
    flushSaves(rootData.get());

    const QDir newCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    CHECK(QFileInfo::exists(newCaveDir.absoluteFilePath(
                                QStringLiteral("trips/%1/%1.cwtrip").arg(kTripName))));
    CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();
    CHECK(loadedRoot->project()->errorModel()->isEmpty());

    cwCave* loadedFolder = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName);
    REQUIRE(loadedFolder != nullptr);
    cwCave* loadedCave = childNamed(loadedFolder, kCaveName);
    REQUIRE(loadedCave != nullptr);
    cwTrip* loadedTrip = tripNamed(loadedCave, kTripName);
    REQUIRE(loadedTrip != nullptr);
    CHECK(loadedTrip->chunkCount() == chunkCount);
}

TEST_CASE("cwSaveLoad restores a deduplicated descriptor name when the move is undone",
          "[cwSaveLoad][MoveNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    const MoveFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("dedup-undo"));

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QDir oldCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    const QStringList filesBefore = relativeFiles(oldCaveDir);

    auto region = rootData->project()->cavingRegion();
    cwCave* twin = addNode(region, fixture.folder, cwSurveyNode::Kind::Cave, kCaveName);
    flushSaves(rootData.get());
    const QStringList twinFiles = relativeFiles(ProjectFilenameTestHelper::dir(twin));

    region->moveNode(fixture.cave, fixture.folder, 1);
    flushSaves(rootData.get());

    const QString dedupedName = fixture.cave->name();
    REQUIRE(dedupedName != kCaveName);

    rootData->undoStack()->undo();
    flushSaves(rootData.get());

    CHECK(fixture.cave->name() == kCaveName);
    CHECK(ProjectFilenameTestHelper::dir(fixture.cave).absolutePath() == oldCaveDir.absolutePath());
    CHECK(relativeFiles(oldCaveDir) == filesBefore);
    CHECK(relativeFiles(dataRoot).filter(dedupedName + QStringLiteral(".cwcave")).isEmpty());
    CHECK(relativeFiles(ProjectFilenameTestHelper::dir(twin)) == twinFiles);
    CHECK(reloadedNodePaths(projectFile) == nodePaths(region));

    rootData->undoStack()->redo();
    flushSaves(rootData.get());

    CHECK(fixture.cave->name() == dedupedName);
    const QDir redoneDir = ProjectFilenameTestHelper::dir(fixture.cave);
    CHECK(redoneDir.absolutePath()
          == dataRoot.absoluteFilePath(QStringLiteral("%1/sub/%2").arg(kFolderName, dedupedName)));
    QStringList expectedFiles = filesBefore;
    expectedFiles.replaceInStrings(kCaveName + QStringLiteral(".cwcave"),
                                   dedupedName + QStringLiteral(".cwcave"));
    expectedFiles.sort();
    CHECK(relativeFiles(redoneDir) == expectedFiles);
    CHECK_FALSE(QFileInfo::exists(oldCaveDir.absolutePath()));
    CHECK(reloadedNodePaths(projectFile) == nodePaths(region));
}

TEST_CASE("cwSaveLoad moves a node beside a sibling whose name differs only in forbidden characters",
          "[cwSaveLoad][MoveNode][NameCollision]")
{
    static const QString kBangName = QStringLiteral("Side Cave!");
    static const QString kQuestionName = QStringLiteral("Side Cave?");

    auto rootData = std::make_unique<cwRootData>();
    auto region = rootData->project()->cavingRegion();

    cwCave* folder = addNode(region, nullptr, cwSurveyNode::Kind::Folder, kFolderName);
    cwCave* sibling = addNode(region, folder, cwSurveyNode::Kind::Cave, kBangName);
    addTrip(sibling, kTripName, QStringLiteral("A"));

    cwCave* mover = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kQuestionName);
    addTrip(mover, kSectionTripName, QStringLiteral("B"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("forbidden-chars"));

    const QDir dataRoot = rootData->project()->dataRootDir();

    region->moveNode(mover, folder, 1);
    flushSaves(rootData.get());

    //Two nodes whose names sanitize to the same file name still get a directory
    //each, and each of those holds its own descriptor.
    const QDir subDir(dataRoot.absoluteFilePath(kFolderName + QStringLiteral("/sub")));
    const QStringList nodeDirs = subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    INFO("Directories under sub/: " << nodeDirs.join(QStringLiteral(", ")).toStdString());
    CHECK(nodeDirs.size() == 2);
    for (const QString& nodeDir : nodeDirs) {
        INFO("Node directory " << nodeDir.toStdString());
        CHECK(QFileInfo::exists(subDir.absoluteFilePath(
                                    QStringLiteral("%1/%1.cwcave").arg(nodeDir))));
    }

    CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(sibling)));
    CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(mover)));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    int renameWarnings = 0;
    const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
    for (const cwError& error : errors) {
        INFO("Error: " << error.message().toStdString());
        if (error.message().contains(QStringLiteral("renamed to"))) {
            renameWarnings++;
        }
    }
    CHECK(renameWarnings == 0);
    CHECK(reloadedNodePaths(projectFile) == nodePaths(region));
}

TEST_CASE("cwSaveLoad carries attachments of a nested node through a move",
          "[cwSaveLoad][MoveNode][ExternalCenterline]")
{
    const QString sourcePath =
            testcasesDatasetSourcePath(QStringLiteral("external-centerlines/survex_simple.svx"));
    REQUIRE(QFileInfo::exists(sourcePath));
    const QString sourceName = QFileInfo(sourcePath).fileName();

    auto rootData = std::make_unique<cwRootData>();
    const MoveFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("move-attachments"));

    //Both attachment owners the layout knows: the trip and the node above it.
    auto tripAttach = rootData->externalCenterlineManager()->attachCenterline(fixture.trip, sourcePath);
    REQUIRE(AsyncFuture::waitForFinished(tripAttach, kAttachTimeoutMs));
    REQUIRE_FALSE(tripAttach.result().hasError());

    //A cave-level attach builds the cave's Scope trips, so it runs on a node
    //that holds none — here a child of the node about to move.
    //The node-level twin of that attachment, on a child of the node about to
    //move: the copied file beside the descriptor, and the owner-relative entry
    //file the model resolves it through.
    cwCave* scopeNode = addNode(rootData->project()->cavingRegion(), fixture.cave,
                                cwSurveyNode::Kind::Cave, kSectionName);
    flushSaves(rootData.get());
    writeFile(ProjectFilenameTestHelper::externalCenterlineDir(scopeNode).absoluteFilePath(sourceName),
              readFile(sourcePath));
    scopeNode->setExternalCenterline(cwExternalCenterline(sourceName));

    flushSaves(rootData.get());

    REQUIRE(QFileInfo::exists(
                ProjectFilenameTestHelper::externalCenterlineDir(fixture.trip).absoluteFilePath(sourceName)));
    REQUIRE(QFileInfo::exists(
                ProjectFilenameTestHelper::externalCenterlineDir(scopeNode).absoluteFilePath(sourceName)));
    const QByteArray sourceBytes = readFile(sourcePath);

    rootData->project()->cavingRegion()->moveNode(fixture.cave, fixture.folder, 0);
    flushSaves(rootData.get());

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QDir movedCaveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    CHECK(movedCaveDir.absolutePath()
          == dataRoot.absoluteFilePath(QStringLiteral("%1/sub/%2").arg(kFolderName, kCaveName)));

    //The attachment travels with its owner, and the owner-relative path the
    //model stores still names it at the new location.
    const QString movedCaveAttachment =
            ProjectFilenameTestHelper::externalCenterlineDir(scopeNode).absoluteFilePath(sourceName);
    const QString movedTripAttachment =
            ProjectFilenameTestHelper::externalCenterlineDir(fixture.trip).absoluteFilePath(sourceName);
    CHECK(QFileInfo::exists(movedCaveAttachment));
    CHECK(QFileInfo::exists(movedTripAttachment));
    CHECK(readFile(movedCaveAttachment) == sourceBytes);
    CHECK(readFile(movedTripAttachment) == sourceBytes);
    CHECK(movedCaveAttachment.startsWith(movedCaveDir.absolutePath()));
    CHECK(movedTripAttachment.startsWith(movedCaveDir.absolutePath()));

    //The model still names each attachment the same way, owner-relative.
    CHECK(scopeNode->externalCenterline().entryFile() == sourceName);
    CHECK(fixture.trip->externalCenterline().entryFile() == sourceName);

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();
    CHECK(loadedRoot->project()->errorModel()->isEmpty());

    cwCave* loadedFolder = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName);
    REQUIRE(loadedFolder != nullptr);
    cwCave* loadedCave = childNamed(loadedFolder, kCaveName);
    REQUIRE(loadedCave != nullptr);
    cwCave* loadedScopeNode = childNamed(loadedCave, kSectionName);
    REQUIRE(loadedScopeNode != nullptr);
    CHECK(QFileInfo::exists(
              ProjectFilenameTestHelper::externalCenterlineDir(loadedScopeNode).absoluteFilePath(sourceName)));
    CHECK(QFileInfo::exists(
              ProjectFilenameTestHelper::externalCenterlineDir(tripNamed(loadedCave, kTripName))
              .absoluteFilePath(sourceName)));
}
