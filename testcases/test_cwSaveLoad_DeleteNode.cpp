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
#include "cwZip.h"
#include "cwRootData.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "LoadProjectHelper.h"
#include "ProjectFilenameTestHelper.h"
#include "SurveyTreeSyncFixture.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"

//Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QEvent>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUrl>
#include <QUuid>

using namespace SurveyTreeTestHelper;

namespace {

const QString kFolderName = QStringLiteral("Kentucky field seasons");
const QString kCaveName = QStringLiteral("Side Cave");
const QString kTripName = QStringLiteral("Sump dig");
const QString kKeptCaveName = QStringLiteral("Kept Cave");
const QString kAttachmentName = QStringLiteral("entry.svx");
const QString kAttachmentContent = QStringLiteral("*begin entry\n*end entry\n");
const QString kTrashDirName = QStringLiteral(".cw_trash");
constexpr int kGitTimeoutMs = 30000;

//! The project root, which holds the data root and the trash side by side.
QDir projectRootDir(const cwProject* project)
{
    QDir dir = project->dataRootDir();
    dir.cdUp();
    return dir;
}

//! The directories the trash holds, one per trashed node or trip.
QStringList trashEntries(const QDir& trashDir)
{
    return trashDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}

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

//! Folder "Kentucky field seasons" holding "Side Cave" — one trip with a real
//! note image and a planted cave-level attachment — beside a second nested node
//! that keeps the project from going flat when the first one is deleted.
struct DeleteFixture {
    cwCave* folder = nullptr;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;
    QString noteImageName;
    QByteArray noteImageBytes;
};

DeleteFixture buildFixture(cwRootData* rootData)
{
    auto region = rootData->project()->cavingRegion();

    DeleteFixture fixture;
    fixture.folder = addNode(region, nullptr, cwSurveyNode::Kind::Folder, kFolderName);
    fixture.cave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kCaveName);
    fixture.trip = addTrip(fixture.cave, kTripName, QStringLiteral("A"));

    const QString noteImagePath =
            copyToTempFolder(testcasesDatasetPath("test_cwAddImageTask/supportedImage.png"));
    fixture.trip->notes()->addFromFiles({QUrl::fromLocalFile(noteImagePath)});
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(fixture.trip->notes()->rowCount() == 1);

    //A second child of the folder, so deleting the first one leaves the project
    //with hierarchy: this case is about the subtree's files, not the restamp a
    //flattening delete would also queue.
    addNode(region, fixture.folder, cwSurveyNode::Kind::Cave, kKeptCaveName);

    return fixture;
}

}

TEST_CASE("cwSaveLoad restores a deleted node's whole subtree when the delete is undone",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    DeleteFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("delete-node"));

    writeFile(ProjectFilenameTestHelper::externalCenterlineDir(fixture.cave)
              .absoluteFilePath(kAttachmentName),
              kAttachmentContent.toUtf8());

    rootData->project()->cavingRegion()->moveNode(fixture.cave, fixture.folder, 0);
    flushSaves(rootData.get());

    const QDir dataRoot = rootData->project()->dataRootDir();
    const QDir caveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    REQUIRE(caveDir.absolutePath()
            == dataRoot.absoluteFilePath(QStringLiteral("%1/sub/%2").arg(kFolderName, kCaveName)));

    fixture.noteImageName = fixture.trip->notes()->notes().first()->image().path();
    const QString noteImagePath =
            ProjectFilenameTestHelper::absolutePath(fixture.trip->notes()->notes().first(),
                                                    fixture.noteImageName);
    REQUIRE(QFileInfo::exists(noteImagePath));
    fixture.noteImageBytes = readFile(noteImagePath);

    const QStringList filesBefore = relativeFiles(caveDir);
    REQUIRE(filesBefore.contains(QStringLiteral("external-centerline/") + kAttachmentName));

    const auto checkSubtreeIsBack = [&]() {
        CHECK(QFileInfo::exists(caveDir.absoluteFilePath(kCaveName + QStringLiteral(".cwcave"))));
        CHECK(QFileInfo::exists(caveDir.absoluteFilePath(
                                    QStringLiteral("trips/%1/%1.cwtrip").arg(kTripName))));

        //The subtree's payload is the part a Remove of the directory takes with
        //it: a note image and an attachment are written once, by the verb that
        //made them, and a re-announced object rewrites only its descriptor.
        const QString restoredImagePath =
                caveDir.absoluteFilePath(QStringLiteral("trips/%1/notes/%2")
                                         .arg(kTripName, fixture.noteImageName));
        CHECK(QFileInfo::exists(restoredImagePath));
        if (QFileInfo::exists(restoredImagePath)) {
            CHECK(readFile(restoredImagePath) == fixture.noteImageBytes);
        }
        CHECK(QFileInfo::exists(caveDir.absoluteFilePath(
                                    QStringLiteral("external-centerline/") + kAttachmentName)));
        CHECK(relativeFiles(caveDir) == filesBefore);

        auto loadedRoot = std::make_unique<cwRootData>();
        addTokenManager(loadedRoot->project());
        loadedRoot->project()->loadOrConvert(projectFile);
        loadedRoot->project()->waitLoadToFinish();

        QStringList messages;
        const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
        for (const cwError& error : errors) {
            messages.append(error.message());
        }
        INFO("Errors: " << messages.join(QStringLiteral(" | ")).toStdString());
        CHECK(loadedRoot->project()->errorModel()->isEmpty());

        cwCave* loadedFolder = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName);
        REQUIRE(loadedFolder != nullptr);
        cwCave* loadedCave = childNamed(loadedFolder, kCaveName);
        REQUIRE(loadedCave != nullptr);
        cwTrip* loadedTrip = tripNamed(loadedCave, kTripName);
        REQUIRE(loadedTrip != nullptr);
        CHECK(loadedTrip->notes()->rowCount() == 1);
    };

    SECTION("the delete reached disk before the undo")
    {
        fixture.folder->removeNode(fixture.folder->indexOfNode(fixture.cave));
        flushSaves(rootData.get());
        CHECK_FALSE(QFileInfo::exists(caveDir.absolutePath()));

        rootData->undoStack()->undo();
        flushSaves(rootData.get());

        checkSubtreeIsBack();
    }

    SECTION("the undo arrives before the queue drains")
    {
        fixture.folder->removeNode(fixture.folder->indexOfNode(fixture.cave));
        rootData->undoStack()->undo();
        flushSaves(rootData.get());

        checkSubtreeIsBack();
    }
}

TEST_CASE("cwSaveLoad restores a deleted trip's directory when the delete is undone",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    DeleteFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("delete-trip"));

    const QDir caveDir = ProjectFilenameTestHelper::dir(fixture.cave);
    const QString noteImageName = fixture.trip->notes()->notes().first()->image().path();
    const QString noteImagePath =
            ProjectFilenameTestHelper::absolutePath(fixture.trip->notes()->notes().first(),
                                                    noteImageName);
    REQUIRE(QFileInfo::exists(noteImagePath));
    const QByteArray noteImageBytes = readFile(noteImagePath);
    const QStringList filesBefore = relativeFiles(caveDir);

    const auto checkTripIsBack = [&]() {
        CHECK(QFileInfo::exists(caveDir.absoluteFilePath(
                                    QStringLiteral("trips/%1/%1.cwtrip").arg(kTripName))));
        CHECK(QFileInfo::exists(noteImagePath));
        if (QFileInfo::exists(noteImagePath)) {
            CHECK(readFile(noteImagePath) == noteImageBytes);
        }
        CHECK(relativeFiles(caveDir) == filesBefore);

        auto loadedRoot = std::make_unique<cwRootData>();
        addTokenManager(loadedRoot->project());
        loadedRoot->project()->loadOrConvert(projectFile);
        loadedRoot->project()->waitLoadToFinish();

        CHECK(loadedRoot->project()->errorModel()->isEmpty());
        cwCave* loadedCave = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kCaveName);
        REQUIRE(loadedCave != nullptr);
        cwTrip* loadedTrip = tripNamed(loadedCave, kTripName);
        REQUIRE(loadedTrip != nullptr);
        CHECK(loadedTrip->notes()->rowCount() == 1);
    };

    SECTION("the delete reached disk before the undo")
    {
        fixture.cave->removeTrip(0);
        flushSaves(rootData.get());
        CHECK_FALSE(QFileInfo::exists(noteImagePath));

        rootData->undoStack()->undo();
        flushSaves(rootData.get());

        checkTripIsBack();
    }

    SECTION("the undo arrives before the queue drains")
    {
        fixture.cave->removeTrip(0);
        rootData->undoStack()->undo();
        flushSaves(rootData.get());

        checkTripIsBack();
    }
}

TEST_CASE("cwSaveLoad removes a trashed directory when the delete leaves the undo stack",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    DeleteFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("delete-forever"));

    const QDir trashDir(projectRootDir(rootData->project()).absoluteFilePath(kTrashDirName));

    rootData->project()->cavingRegion()->rootNode()->removeNode(
                rootData->project()->cavingRegion()->rootNode()->indexOfNode(fixture.cave));
    flushSaves(rootData.get());

    //The delete is undoable, so the subtree is waiting in the trash rather
    //than gone. The tree announces a subtree's rows deepest-first, so the
    //trip's directory is an entry of its own beside the node's.
    REQUIRE_FALSE(trashEntries(trashDir).isEmpty());

    //Clearing the stack destroys the removed objects, and nothing can ask for
    //the delete back any more.
    rootData->undoStack()->clear();

    CHECK(waitUntil([&]() {
        //The command deleteLater()s what it owned, and a deferred delete posted
        //outside an event loop waits for one.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        flushSaves(rootData.get());
        return trashEntries(trashDir).isEmpty();
    }));
}

TEST_CASE("cwSaveLoad opens a project without what a crash left in the trash",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    DeleteFixture fixture = buildFixture(rootData.get());
    Q_UNUSED(fixture);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("stale-trash"));

    const QDir projectRoot = projectRootDir(rootData->project());
    const QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString staleName = QStringLiteral("Crashed Cave");
    const QString staleDir = projectRoot.absoluteFilePath(
                QStringLiteral("%1/%2/%3").arg(kTrashDirName, entryId, staleName));
    writeFile(QDir(staleDir).absoluteFilePath(staleName + QStringLiteral(".cwcave")),
              QByteArray("{}"));

    rootData.reset();

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    CHECK(loadedRoot->project()->errorModel()->isEmpty());
    CHECK(childNamed(loadedRoot->project()->cavingRegion()->rootNode(), staleName) == nullptr);
    CHECK(childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName) != nullptr);

    //Opening the project is what empties the trash: a crash between a delete
    //and a close leaves entries no undo stack can reach.
    const QDir trashDir(projectRoot.absoluteFilePath(kTrashDirName));
    CHECK(waitUntil([&]() {
        flushSaves(loadedRoot.get());
        return trashEntries(trashDir).isEmpty();
    }));
}

TEST_CASE("cwSaveLoad keeps a trashed directory out of git and out of a bundle",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    DeleteFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("trash-exclusion"));

    const QDir projectRoot = projectRootDir(rootData->project());
    rootData->project()->cavingRegion()->rootNode()->removeNode(
                rootData->project()->cavingRegion()->rootNode()->indexOfNode(fixture.cave));
    flushSaves(rootData.get());

    const QDir trashDir(projectRoot.absoluteFilePath(kTrashDirName));
    REQUIRE_FALSE(trashEntries(trashDir).isEmpty());

    SECTION("git never sees it")
    {
        const QString gitPath = QStandardPaths::findExecutable(QStringLiteral("git"));
        if (gitPath.isEmpty()) {
            SUCCEED("git is not installed, so there is no working tree to ask");
        } else {
            QProcess git;
            git.setWorkingDirectory(projectRoot.absolutePath());
            git.start(gitPath, {QStringLiteral("status"), QStringLiteral("--porcelain")});
            REQUIRE(git.waitForFinished(kGitTimeoutMs));
            REQUIRE(git.exitCode() == 0);
            const QString status = QString::fromUtf8(git.readAllStandardOutput());
            INFO("git status: " << status.toStdString());
            //The delete itself is what git has to say something about, so an
            //empty answer would mean the question went unasked.
            CHECK_FALSE(status.trimmed().isEmpty());
            CHECK_FALSE(status.contains(kTrashDirName));
        }
    }

    SECTION("a bundled save never carries it")
    {
        const QString bundlePath =
                QDir(tempDir.path()).absoluteFilePath(QStringLiteral("trash-exclusion.cw"));
        REQUIRE(rootData->project()->saveAs(bundlePath));
        flushSaves(rootData.get());
        REQUIRE(QFileInfo::exists(bundlePath));

        QTemporaryDir extractDir;
        REQUIRE(extractDir.isValid());
        const auto extractResult = cwZip::extractAll(bundlePath, extractDir.path());
        REQUIRE_FALSE(extractResult.hasError());
        CHECK_FALSE(QFileInfo::exists(QDir(extractDir.path()).absoluteFilePath(kTrashDirName)));
    }
}

TEST_CASE("cwSaveLoad keeps a note's own delete when the node above it is deleted and undone",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    DeleteFixture fixture = buildFixture(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("delete-note-too"));

    const QString noteImageName = fixture.trip->notes()->notes().first()->image().path();
    const QString noteImagePath =
            ProjectFilenameTestHelper::absolutePath(fixture.trip->notes()->notes().first(),
                                                    noteImageName);
    const QString noteDescriptorPath =
            ProjectFilenameTestHelper::absolutePath(fixture.trip->notes()->notes().first());
    REQUIRE(QFileInfo::exists(noteImagePath));
    REQUIRE(QFileInfo::exists(noteDescriptorPath));

    //The note's own delete is still waiting in the queue when the node above
    //it is deleted, so the move into the trash carries the note's files away
    //with everything else.
    fixture.trip->notes()->removeNote(0);
    rootData->project()->cavingRegion()->rootNode()->removeNode(
                rootData->project()->cavingRegion()->rootNode()->indexOfNode(fixture.cave));

    //Only the node's delete is undoable, so the note stays deleted.
    rootData->undoStack()->undo();
    flushSaves(rootData.get());

    CHECK(QFileInfo::exists(
              ProjectFilenameTestHelper::dir(fixture.cave)
              .absoluteFilePath(QStringLiteral("trips/%1/%1.cwtrip").arg(kTripName))));
    CHECK_FALSE(QFileInfo::exists(noteDescriptorPath));

    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    CHECK(loadedRoot->project()->errorModel()->isEmpty());
    cwCave* loadedCave = childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kCaveName);
    REQUIRE(loadedCave != nullptr);
    cwTrip* loadedTrip = tripNamed(loadedCave, kTripName);
    REQUIRE(loadedTrip != nullptr);
    CHECK(loadedTrip->notes()->rowCount() == 0);
}

TEST_CASE("cwSaveLoad keeps a temporary project's trash out of where it is saved",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    DeleteFixture fixture = buildFixture(rootData.get());
    flushSaves(rootData.get());

    //The project is still temporary, so this delete fills the trash of the
    //root that Save As is about to move.
    rootData->project()->cavingRegion()->rootNode()->removeNode(
                rootData->project()->cavingRegion()->rootNode()->indexOfNode(fixture.cave));
    flushSaves(rootData.get());

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile =
            saveProjectAs(rootData.get(), QDir(tempDir.path()), QStringLiteral("saved-from-temp"));

    const QDir projectRoot = projectRootDir(rootData->project());
    CHECK_FALSE(QFileInfo::exists(projectRoot.absoluteFilePath(kTrashDirName)));

    //The saved project holds what the user kept, and nothing the delete took.
    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    CHECK(loadedRoot->project()->errorModel()->isEmpty());
    CHECK(childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kCaveName) == nullptr);
    CHECK(childNamed(loadedRoot->project()->cavingRegion()->rootNode(), kFolderName) != nullptr);
}

TEST_CASE("cwSaveLoad keeps a re-created node's directory when a sync follows the delete",
          "[cwSaveLoad][NodeTree][DeleteNode]")
{
    using namespace SurveyTreeSyncFixture;

    auto clones = makeTwoClones(true);

    //The node beside "Side Cave" — whatever the merges below have named it.
    const auto siblingOf = [](cwProject* project) {
        const QList<cwSurveyNode*> children = folderOf(project->cavingRegion())->childNodes();
        for (cwSurveyNode* child : children) {
            if (child->name() != kSideCaveName) {
                return qobject_cast<cwCave*>(child);
            }
        }
        return static_cast<cwCave*>(nullptr);
    };

    //Both sides rename the same node and both push. The merge that resolves it
    //is what gives every object a loaded path — the path a later merge asks
    //about, and the one a delete leaves naming a live directory.
    const auto conflictingRound = [&](const QString& authorName, const QString& peerName) {
        cwCave* authorSibling = siblingOf(clones->authorProject());
        REQUIRE(authorSibling != nullptr);
        authorSibling->setName(authorName);
        clones->authorProject()->waitSaveToFinish();
        syncAuthor(clones.get());

        cwCave* peerSibling = siblingOf(clones->peerProject());
        REQUIRE(peerSibling != nullptr);
        peerSibling->setName(peerName);
        pushPeer(clones.get());
    };

    conflictingRound(kAuthorSibling, kPeerSideCave);

    //The peer deletes a node and puts a new one of the same name in its place.
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    cwCave* peerSideCave = SurveyTreeSyncFixture::childNamed(peerFolder, kSideCaveName);
    REQUIRE(peerSideCave != nullptr);
    peerFolder->removeNode(peerFolder->indexOfNode(peerSideCave));

    cwCave* recreated = addNode(peerRegion, peerFolder, cwSurveyNode::Kind::Cave, kSideCaveName);
    const QUuid recreatedId = recreated->id();
    clones->peerRootData->futureManagerModel()->waitForFinished();
    clones->peerProject()->waitSaveToFinish();
    REQUIRE(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(recreated)));

    conflictingRound(QStringLiteral("Author renamed again"), QStringLiteral("Peer renamed again"));

    //The deleted node's directory is in the trash. What stands in its place on
    //disk belongs to the node the user has just made, and a sync may only
    //rename it — never take it away.
    cwCave* stillThere = nullptr;
    const QList<cwSurveyNode*> children = folderOf(clones->peerProject()->cavingRegion())->childNodes();
    for (cwSurveyNode* child : children) {
        if (child->id() == recreatedId) {
            stillThere = qobject_cast<cwCave*>(child);
        }
    }
    REQUIRE(stillThere != nullptr);
    CHECK(QFileInfo::exists(ProjectFilenameTestHelper::absolutePath(stillThere)));
}
