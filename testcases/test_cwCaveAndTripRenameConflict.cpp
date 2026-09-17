// Catch2 includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

// Our includes
#include "LoadProjectHelper.h"
#include "SurveyTreeSyncFixture.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwErrorListModel.h"
#include "cwNote.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModel.h"
#include "SurveyTreeTestHelper.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwTrip.h"
#include "GitRepository.h"
#include "asyncfuture.h"

// Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>

#include <QDirIterator>
#include <QStringList>

#include <algorithm>

// libgit2
#include "git2.h"

using namespace SurveyTreeSyncFixture;

// ---------------------------------------------------------------------------
// Scenario A — Cave rename/rename conflict
//
// base:   one cave named "Conflict Cave"
// local:  cave renamed → "Author Cave"   (committed, not yet pushed)
// remote: cave renamed → "Peer Cave"     (peer pushed)
// action: local syncs → ours wins, and the losing "Peer Cave" directory is
//         removed by cwSurveyNodeSyncMergeHandler's orphan cleanup.
// ---------------------------------------------------------------------------

TEST_CASE("Local cave rename wins on concurrent rename-rename conflict",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    static const QString kOriginalProjectName = QStringLiteral("OriginalProject");
    static const QString kAuthorCave          = QStringLiteral("Author Cave");
    static const QString kPeerCave            = QStringLiteral("Peer Cave");
    static const QString kBaseCave            = QStringLiteral("Conflict Cave");

    // --- Author creates initial project ---
    QTemporaryDir authorProjectDir;
    QTemporaryDir remoteRoot;
    QTemporaryDir cloneDir;
    REQUIRE(authorProjectDir.isValid());
    REQUIRE(remoteRoot.isValid());
    REQUIRE(cloneDir.isValid());

    auto authorRootData = std::make_unique<cwRootData>();
    auto* authorProject = authorRootData->project();
    authorRootData->account()->setName(QStringLiteral("Author User"));
    authorRootData->account()->setEmail(QStringLiteral("author@example.com"));

    auto* region = authorProject->cavingRegion();
    region->setName(kOriginalProjectName);
    region->addCave();
    REQUIRE(region->cave(0) != nullptr);
    region->cave(0)->setName(kBaseCave);

    const QString projectPath =
        QDir(authorProjectDir.path()).filePath(kOriginalProjectName + QStringLiteral(".cwproj"));
    REQUIRE(authorProject->saveAs(projectPath));
    authorProject->waitSaveToFinish();

    // --- Author pushes to bare remote ---
    const QString remoteRepoPath =
        QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    REQUIRE(initBareRepo(remoteRepoPath) == GIT_OK);

    REQUIRE(authorProject->repository()->addRemote(QStringLiteral("origin"),
                                                   QUrl::fromLocalFile(remoteRepoPath)).isEmpty());

    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    CHECK(authorProject->errorModel()->count() == 0);

    // --- Peer clones, renames cave to kPeerCave, pushes ---
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("peer-clone"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(authorRootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error: " << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    auto peerRootData = std::make_unique<cwRootData>();
    peerRootData->account()->setName(QStringLiteral("Peer User"));
    peerRootData->account()->setEmail(QStringLiteral("peer@example.com"));

    auto* peerProject = peerRootData->project();
    const QString peerProjectPath =
        QDir(clonePath).filePath(QFileInfo(projectPath).fileName());
    REQUIRE(QFileInfo::exists(peerProjectPath));
    peerProject->loadFile(peerProjectPath);
    peerProject->waitLoadToFinish();
    peerProject->waitSaveToFinish();

    auto* peerRepository = peerProject->repository();
    REQUIRE(peerRepository != nullptr);
    peerRepository->setAccount(peerRootData->account());

    REQUIRE(peerProject->cavingRegion()->caveCount() == 1);
    peerProject->cavingRegion()->cave(0)->setName(kPeerCave);
    peerProject->waitSaveToFinish();
    REQUIRE(isProjectModified(peerProject));

    peerProject->errorModel()->clear();
    REQUIRE(peerProject->sync());
    peerRootData->futureManagerModel()->waitForFinished();
    peerProject->waitSaveToFinish();
    CHECK(peerProject->errorModel()->count() == 0);

    // --- Author renames cave to kAuthorCave locally (without syncing) ---
    REQUIRE(authorProject->cavingRegion()->caveCount() == 1);
    QPointer<cwCave> authorCavePtr = authorProject->cavingRegion()->cave(0);
    REQUIRE(authorCavePtr != nullptr);
    authorCavePtr->setName(kAuthorCave);
    authorProject->waitSaveToFinish();
    REQUIRE(isProjectModified(authorProject));

    // --- Author syncs: commits local rename, pulls peer rename, ours wins ---
    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();

    CHECK(authorProject->errorModel()->count() == 0);

    // The in-memory cave must reflect the author's rename (ours wins).
    REQUIRE(authorCavePtr != nullptr);
    CHECK(authorCavePtr->name() == kAuthorCave);

    // Derive the data root directory from the git repo root.
    const QDir repoRoot = QFileInfo(authorProject->filename()).absoluteDir();
    const QDir dataRoot(repoRoot.absoluteFilePath(kOriginalProjectName));

    // Winning cave directory and descriptor must exist.
    CHECK(dataRoot.exists());
    CHECK(QFileInfo::exists(dataRoot.absoluteFilePath(kAuthorCave)));
    CHECK(QFileInfo::exists(
        dataRoot.absoluteFilePath(kAuthorCave + QChar('/') + kAuthorCave + QStringLiteral(".cwcave"))));

    // Losing cave directory and descriptor must NOT remain on disk.
    CHECK_FALSE(QFileInfo::exists(dataRoot.absoluteFilePath(kPeerCave)));
    CHECK_FALSE(QFileInfo::exists(
        dataRoot.absoluteFilePath(kPeerCave + QChar('/') + kPeerCave + QStringLiteral(".cwcave"))));
}

// ---------------------------------------------------------------------------
// Scenario B — Trip rename/rename conflict
//
// base:   cave "Cave A", trip "Base Trip"
// local:  trip renamed → "Author Trip"   (committed, not yet pushed)
// remote: trip renamed → "Peer Trip"     (peer pushed)
// action: local syncs → ours wins, and the losing "Peer Trip" directory is
//         removed by cwTripSyncMergeHandler's orphan cleanup.
// ---------------------------------------------------------------------------

TEST_CASE("Local trip rename wins on concurrent rename-rename conflict",
          "[cwTripSyncMergeHandler][sync]")
{
    static const QString kOriginalProjectName = QStringLiteral("OriginalProject");
    static const QString kCaveName            = QStringLiteral("Cave A");
    static const QString kAuthorTrip          = QStringLiteral("Author Trip");
    static const QString kPeerTrip            = QStringLiteral("Peer Trip");
    static const QString kBaseTrip            = QStringLiteral("Base Trip");

    // --- Author creates initial project with one cave and one trip ---
    QTemporaryDir authorProjectDir;
    QTemporaryDir remoteRoot;
    QTemporaryDir cloneDir;
    REQUIRE(authorProjectDir.isValid());
    REQUIRE(remoteRoot.isValid());
    REQUIRE(cloneDir.isValid());

    auto authorRootData = std::make_unique<cwRootData>();
    auto* authorProject = authorRootData->project();
    authorRootData->account()->setName(QStringLiteral("Author User"));
    authorRootData->account()->setEmail(QStringLiteral("author@example.com"));

    auto* region = authorProject->cavingRegion();
    region->setName(kOriginalProjectName);
    region->addCave();
    REQUIRE(region->cave(0) != nullptr);
    region->cave(0)->setName(kCaveName);

    auto* baseTrip = new cwTrip();
    baseTrip->setName(kBaseTrip);
    region->cave(0)->addTrip(baseTrip);
    REQUIRE(region->cave(0)->tripCount() == 1);

    const QString projectPath =
        QDir(authorProjectDir.path()).filePath(kOriginalProjectName + QStringLiteral(".cwproj"));
    REQUIRE(authorProject->saveAs(projectPath));
    authorProject->waitSaveToFinish();

    // --- Author pushes to bare remote ---
    const QString remoteRepoPath =
        QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    REQUIRE(initBareRepo(remoteRepoPath) == GIT_OK);

    REQUIRE(authorProject->repository()->addRemote(QStringLiteral("origin"),
                                                   QUrl::fromLocalFile(remoteRepoPath)).isEmpty());

    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    CHECK(authorProject->errorModel()->count() == 0);

    // --- Peer clones, renames trip to kPeerTrip, pushes ---
    const QString clonePath = QDir(cloneDir.path()).filePath(QStringLiteral("peer-clone"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(authorRootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(remoteRepoPath));
    REQUIRE(AsyncFuture::waitForFinished(cloneFuture, 10000));
    INFO("Clone error: " << cloneFuture.result().errorMessage().toStdString());
    REQUIRE(!cloneFuture.result().hasError());

    auto peerRootData = std::make_unique<cwRootData>();
    peerRootData->account()->setName(QStringLiteral("Peer User"));
    peerRootData->account()->setEmail(QStringLiteral("peer@example.com"));

    auto* peerProject = peerRootData->project();
    const QString peerProjectPath =
        QDir(clonePath).filePath(QFileInfo(projectPath).fileName());
    REQUIRE(QFileInfo::exists(peerProjectPath));
    peerProject->loadFile(peerProjectPath);
    peerProject->waitLoadToFinish();
    peerProject->waitSaveToFinish();

    auto* peerRepository = peerProject->repository();
    REQUIRE(peerRepository != nullptr);
    peerRepository->setAccount(peerRootData->account());

    REQUIRE(peerProject->cavingRegion()->caveCount() == 1);
    REQUIRE(peerProject->cavingRegion()->cave(0)->tripCount() == 1);
    peerProject->cavingRegion()->cave(0)->trip(0)->setName(kPeerTrip);
    peerProject->waitSaveToFinish();
    REQUIRE(isProjectModified(peerProject));

    peerProject->errorModel()->clear();
    REQUIRE(peerProject->sync());
    peerRootData->futureManagerModel()->waitForFinished();
    peerProject->waitSaveToFinish();
    CHECK(peerProject->errorModel()->count() == 0);

    // --- Author renames trip to kAuthorTrip locally (without syncing) ---
    REQUIRE(authorProject->cavingRegion()->caveCount() == 1);
    REQUIRE(authorProject->cavingRegion()->cave(0)->tripCount() == 1);
    QPointer<cwTrip> authorTripPtr = authorProject->cavingRegion()->cave(0)->trip(0);
    REQUIRE(authorTripPtr != nullptr);
    authorTripPtr->setName(kAuthorTrip);
    authorProject->waitSaveToFinish();
    REQUIRE(isProjectModified(authorProject));

    // --- Author syncs: commits local rename, pulls peer rename, ours wins ---
    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();

    CHECK(authorProject->errorModel()->count() == 0);

    // The in-memory trip must reflect the author's rename (ours wins).
    REQUIRE(authorTripPtr != nullptr);
    CHECK(authorTripPtr->name() == kAuthorTrip);

    // Derive the data root directory from the git repo root.
    const QDir repoRoot = QFileInfo(authorProject->filename()).absoluteDir();
    const QDir dataRoot(repoRoot.absoluteFilePath(kOriginalProjectName));
    const QDir caveDir(dataRoot.absoluteFilePath(kCaveName));
    const QDir tripsDir(caveDir.absoluteFilePath(QStringLiteral("trips")));

    // Winning trip directory and descriptor must exist.
    CHECK(tripsDir.exists());
    CHECK(QFileInfo::exists(tripsDir.absoluteFilePath(kAuthorTrip)));
    CHECK(QFileInfo::exists(
        tripsDir.absoluteFilePath(kAuthorTrip + QChar('/') + kAuthorTrip + QStringLiteral(".cwtrip"))));

    // Losing trip directory must NOT remain on disk.
    CHECK_FALSE(QFileInfo::exists(tripsDir.absoluteFilePath(kPeerTrip)));
    CHECK_FALSE(QFileInfo::exists(
        tripsDir.absoluteFilePath(kPeerTrip + QChar('/') + kPeerTrip + QStringLiteral(".cwtrip"))));
}

// ---------------------------------------------------------------------------
// Scenario C — Depth-2 node rename/rename conflict merged from the peer's clone,
// the mirrored direction of the author-side case in
// test_cwSurveyNodeSyncMergeHandler.cpp.
//
// base:   Kentucky field seasons/nodes/Side Cave
// local:  the peer renames it → "Peer Side Cave"
// remote: the author renamed it → "Author Side Cave" and pushed first
// action: the peer syncs. Which name wins is the merge policy's business; what this
//         case pins down is that the winner keeps its depth-2 directory, the loser's
//         own directory is removed, and the sibling subtree is untouched.
// ---------------------------------------------------------------------------

TEST_CASE("Depth-2 node rename/rename leaves one winning directory in the peer's clone",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    auto clones = makeTwoClones(true);

    //--- Author renames the depth-2 node and pushes first ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorFolder = folderOf(authorRegion);
    cwCave* authorSideCave = childNamed(authorFolder, kSideCaveName);
    REQUIRE(authorSideCave != nullptr);
    authorSideCave->setName(kAuthorSideCave);
    clones->authorProject()->waitSaveToFinish();
    syncAuthor(clones.get());

    //--- The peer renames the same node and syncs last ---
    //Which of the two names survives is the merge policy's business; this case pins the
    //depth-N path handling: exactly one name is live, the winner keeps its depth-2
    //directory, the loser's own directory is gone, and the sibling subtree is untouched.
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    QPointer<cwCave> peerSideCave = childNamed(peerFolder, kSideCaveName);
    REQUIRE(peerSideCave != nullptr);
    peerSideCave->setName(kPeerSideCave);
    pushPeer(clones.get());

    REQUIRE(peerSideCave != nullptr);
    const QString winningName = peerSideCave->name();
    INFO("Winning node name: " << winningName.toStdString());
    CHECK((winningName == kPeerSideCave
           || winningName == kAuthorSideCave));
    CHECK(peerSideCave->parentNode() == peerFolder);

    const QString losingName = winningName == kPeerSideCave
                                   ? kAuthorSideCave
                                   : kPeerSideCave;

    const QDir peerRepoRoot = QFileInfo(clones->peerProject()->filename()).absoluteDir();
    const QDir peerDataRoot(peerRepoRoot.absoluteFilePath(kProjectName));
    const QDir peerNodesDir(peerDataRoot.absoluteFilePath(kFolderName + QStringLiteral("/nodes")));

    CHECK(QFileInfo::exists(peerNodesDir.absoluteFilePath(
        winningName + QChar('/') + winningName + QStringLiteral(".cwcave"))));
    CHECK_FALSE(QFileInfo::exists(peerNodesDir.absoluteFilePath(losingName)));

    //The sibling subtree the cleanup must not touch.
    const QDir peerSiblingDir(peerNodesDir.absoluteFilePath(kSiblingCaveName));
    CHECK(peerSiblingDir.exists());
    CHECK(noteImageFiles(peerSiblingDir, kSiblingTripName).size() == 1);
}

// ---------------------------------------------------------------------------
// Scenario D — An ancestor rename concurrent with a descendant trip edit.
// Git merges these as rename + modify per file; the depth-2 subtree must end up
// under the peer's folder name with the author's trip rename intact.
// ---------------------------------------------------------------------------

TEST_CASE("An ancestor rename merges with a concurrent descendant trip edit",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    static const QString kPeerFolder = QStringLiteral("Peer field seasons");
    static const QString kAuthorTripName = QStringLiteral("Author sibling survey");

    auto clones = makeTwoClones(true);

    //--- Peer renames the depth-1 folder and pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    peerFolder->setName(kPeerFolder);
    pushPeer(clones.get());

    //--- Author renames a trip two levels down, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    QPointer<cwCave> authorFolder = folderOf(authorRegion);
    cwCave* authorSibling = childNamed(authorFolder, kSiblingCaveName);
    REQUIRE(authorSibling != nullptr);
    REQUIRE(authorSibling->tripCount() == 1);
    QPointer<cwTrip> authorTrip = authorSibling->trip(0);
    authorTrip->setName(kAuthorTripName);
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    REQUIRE(authorFolder != nullptr);
    CHECK(authorFolder->name() == kPeerFolder);
    REQUIRE(authorTrip != nullptr);
    CHECK(authorTrip->name() == kAuthorTripName);

    //The descendant subtree traveled with the renamed ancestor, note image included.
    const QDir dataRoot = clones->authorDataRoot();
    const QDir siblingDir(dataRoot.absoluteFilePath(
        kPeerFolder + QStringLiteral("/nodes/") + kSiblingCaveName));
    CHECK(siblingDir.exists());
    CHECK(noteImageFiles(siblingDir, kAuthorTripName).size() == 1);

    //The whole subtree moved: not one file is left under the old ancestor name.
    //Git prunes no directory it emptied, so the bare directory chain may linger.
    QStringList leftoverFiles;
    QDirIterator leftovers(dataRoot.absoluteFilePath(kFolderName),
                           QDir::Files | QDir::NoDotAndDotDot,
                           QDirIterator::Subdirectories);
    while (leftovers.hasNext()) {
        leftoverFiles.append(dataRoot.relativeFilePath(leftovers.next()));
    }
    INFO("Left under the old ancestor name: "
         << leftoverFiles.join(QStringLiteral(", ")).toStdString());
    CHECK(leftoverFiles.isEmpty());
}

// ---------------------------------------------------------------------------
// Scenario E — A peer moves a node while the local side adds a trip under its
// old path. The moved directory is the only copy of that subtree, so orphan
// cleanup must leave it alone even though it no longer sits where the live node
// says it does; and the added trip is never dropped (§6.2 step 5 attaches it to
// the nearest descriptor ancestor). A Move is not a save op until C3.3, so the
// peer's move is made by hand in the clone's working tree.
// ---------------------------------------------------------------------------

TEST_CASE("A peer's node move keeps the moved subtree and a locally added trip",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    static const QString kAddedTripName = QStringLiteral("Added while moved");

    auto clones = makeTwoClones(false);

    //--- Peer moves Kentucky field seasons/nodes/Sibling Cave up to the data root ---
    //Sibling Cave carries a trip with a note image: after the move that image exists
    //nowhere else, so deleting the moved directory would destroy the only copy.
    const QDir peerDataRoot(clones->cloneRepository.directory().absoluteFilePath(kProjectName));
    const QString movedFrom =
        peerDataRoot.absoluteFilePath(kFolderName + QStringLiteral("/nodes/") + kSiblingCaveName);
    const QString movedTo = peerDataRoot.absoluteFilePath(kSiblingCaveName);
    REQUIRE(QFileInfo::exists(movedFrom));
    REQUIRE(QDir().rename(movedFrom, movedTo));

    clones->cloneRepository.commitAll(QStringLiteral("Move Sibling Cave to the data root"), QString());
    auto pushFuture = clones->cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(pushFuture, 10000));
    INFO("Push error: " << pushFuture.result().errorMessage().toStdString());
    REQUIRE(!pushFuture.result().hasError());

    //--- Author adds a trip under the node's old path, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorFolder = folderOf(authorRegion);
    cwCave* authorSibling = childNamed(authorFolder, kSiblingCaveName);
    REQUIRE(authorSibling != nullptr);
    authorSibling->addTrip();
    authorSibling->trip(authorSibling->tripCount() - 1)->setName(kAddedTripName);
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    //The trip attached somewhere in the tree — it is never dropped.
    const QList<cwTrip*> trips = authorRegion->rootNode()->allTrips();
    const bool addedTripSurvived = std::any_of(trips.begin(), trips.end(), [](const cwTrip* trip) {
        return trip != nullptr && trip->name() == kAddedTripName;
    });
    CHECK(addedTripSurvived);

    //The moved subtree is still on disk: the note image has exactly one copy, and the
    //moved node still has a descriptor. Orphan cleanup treats a moved directory as an
    //orphan only when the node's own directory already holds a descriptor.
    const QDir dataRoot = clones->authorDataRoot();
    QStringList noteImages;
    QStringList descriptors;
    QDirIterator files(dataRoot.absolutePath(),
                       QDir::Files | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString relativePath = dataRoot.relativeFilePath(files.next());
        if (relativePath.endsWith(QStringLiteral(".png"))) {
            noteImages.append(relativePath);
        } else if (relativePath.endsWith(QStringLiteral(".cwcave"))) {
            descriptors.append(relativePath);
        }
    }

    INFO("Note images under the data root: " << noteImages.join(QStringLiteral(", ")).toStdString());
    CHECK(noteImages.size() == 1);
    const bool movedNodeHasDescriptor =
        std::any_of(descriptors.begin(), descriptors.end(), [](const QString& path) {
            return path.endsWith(kSiblingCaveName + QStringLiteral(".cwcave"));
        });
    INFO("Descriptors under the data root: " << descriptors.join(QStringLiteral(", ")).toStdString());
    CHECK(movedNodeHasDescriptor);
}

// ---------------------------------------------------------------------------
// Scenario F — A peer deletes a nested node while the local side edits a trip
// inside it. The delete/modify pair has no test at any depth: what this case
// records is where the edited trip ends up and whether its data survives.
// ---------------------------------------------------------------------------

namespace {

//! Every file below \a dir whose name ends in \a suffix, relative to it.
QStringList filesWithSuffix(const QDir& dir, const QString& suffix)
{
    QStringList found;
    QDirIterator it(dir.absolutePath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString relativePath = dir.relativeFilePath(it.next());
        if (relativePath.startsWith(QStringLiteral(".git"))) {
            continue;
        }
        if (relativePath.endsWith(suffix)) {
            found.append(relativePath);
        }
    }
    found.sort();
    return found;
}

//! The messages of every Fatal error a fresh load of \a projectFile reports.
QStringList fatalLoadErrors(const QString& projectFile)
{
    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    QStringList messages;
    const QList<cwError> errors = loadedRoot->project()->errorModel()->toList();
    for (const cwError& error : errors) {
        if (error.type() == cwError::Fatal) {
            messages.append(error.message());
        }
    }
    return messages;
}

//! The bytes of the one note image \a tripName holds under \a nodeDir.
QByteArray noteImageBytes(const QDir& nodeDir, const QString& tripName)
{
    const QDir notesDir(nodeDir.absoluteFilePath(
        QStringLiteral("trips/") + tripName + QStringLiteral("/notes")));
    const QStringList images = notesDir.entryList({QStringLiteral("*.png")}, QDir::Files);
    REQUIRE(images.size() == 1);

    QFile image(notesDir.absoluteFilePath(images.constFirst()));
    REQUIRE(image.open(QIODevice::ReadOnly));
    return image.readAll();
}

//! True when \a diagnostics say the named handler applied on the deterministic path.
bool handlerApplied(const QStringList& diagnostics, const QString& handlerName)
{
    return std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                       [&handlerName](const QString& diagnostic) {
        return diagnostic.contains(QStringLiteral("reconcile handler"))
                && diagnostic.contains(handlerName)
                && diagnostic.contains(QStringLiteral("applied"));
    });
}

bool fellBackToFullReload(const QStringList& diagnostics)
{
    return std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                       [](const QString& diagnostic) {
        return diagnostic.contains(QStringLiteral("reconcile fallback to full reload"));
    });
}

//! The rotation of the one note of the trip named \a tripName, read back from disk.
double reloadedNoteRotation(const QString& projectFile, const QString& tripName)
{
    auto loadedRoot = std::make_unique<cwRootData>();
    addTokenManager(loadedRoot->project());
    loadedRoot->project()->loadOrConvert(projectFile);
    loadedRoot->project()->waitLoadToFinish();

    const QList<cwTrip*> trips = loadedRoot->project()->cavingRegion()->rootNode()->allTrips();
    for (cwTrip* trip : trips) {
        if (trip->name() == tripName) {
            const QList<cwNote*> notes = trip->notes()->notes();
            REQUIRE(notes.size() == 1);
            return notes.constFirst()->rotate();
        }
    }

    FAIL("No trip named " << tripName.toStdString() << " was loaded");
    return 0.0;
}

}

TEST_CASE("A peer's node delete merges with a local edit inside that node",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    auto clones = makeTwoClones(true);

    //--- The peer deletes the depth-2 node and pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    cwCave* peerSibling = childNamed(peerFolder, kSiblingCaveName);
    REQUIRE(peerSibling != nullptr);
    peerFolder->removeNode(peerFolder->indexOfNode(peerSibling));
    pushPeer(clones.get());

    //--- The author adds survey data to a trip inside it, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorFolder = folderOf(authorRegion);
    cwCave* authorSibling = childNamed(authorFolder, kSiblingCaveName);
    REQUIRE(authorSibling != nullptr);
    REQUIRE(authorSibling->tripCount() == 1);
    const QUuid authorSiblingId = authorSibling->id();
    const QDir dataRoot = clones->authorDataRoot();
    const QByteArray originalImageBytes =
        noteImageBytes(QDir(dataRoot.absoluteFilePath(kFolderName + QStringLiteral("/nodes/")
                                                      + kSiblingCaveName)),
                       kSiblingTripName);
    REQUIRE_FALSE(originalImageBytes.isEmpty());

    SurveyTreeTestHelper::addShot(authorSibling->trip(0), QStringLiteral("S1"), QStringLiteral("S2"));
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    //Ours wins whole: the node the peer deleted is still the edited trip's parent, and
    //the delete never reached the model.
    const QList<cwTrip*> trips = authorRegion->rootNode()->allTrips();
    const auto editedTrip = std::find_if(trips.begin(), trips.end(), [](const cwTrip* trip) {
        return trip != nullptr && trip->name() == kSiblingTripName;
    });
    REQUIRE(editedTrip != trips.end());
    const cwSurveyNode* editedTripOwner = (*editedTrip)->parentCave();
    REQUIRE(editedTripOwner != nullptr);
    INFO("The edited trip now hangs under: " << editedTripOwner->name().toStdString());
    CHECK(editedTripOwner->id() == authorSiblingId);
    CHECK((*editedTrip)->chunkCount() >= 1);

    //The note image came back with its bytes, not as an unhydrated LFS pointer.
    const QStringList noteImages = filesWithSuffix(dataRoot, QStringLiteral(".png"));
    INFO("Note images: " << noteImages.join(QStringLiteral(", ")).toStdString());
    REQUIRE(noteImages.size() == 1);
    const QByteArray restoredImageBytes =
        noteImageBytes(QDir(dataRoot.absoluteFilePath(kFolderName + QStringLiteral("/nodes/")
                                                      + kSiblingCaveName)),
                       kSiblingTripName);
    CHECK(restoredImageBytes == originalImageBytes);

    const QStringList fatalErrors = fatalLoadErrors(clones->authorProject()->filename());
    INFO("Fatal load errors: " << fatalErrors.join(QStringLiteral(" | ")).toStdString());
    CHECK(fatalErrors.isEmpty());

    //--- Convergence: the peer's next sync brings the node back with the author's shot ---
    auto* peerProject = clones->peerProject();
    peerProject->errorModel()->clear();
    REQUIRE(peerProject->sync());
    clones->peerRootData->futureManagerModel()->waitForFinished();
    peerProject->waitSaveToFinish();
    CHECK(peerProject->errorModel()->count() == 0);

    cwCave* peerSiblingAgain = childNamed(folderOf(peerRegion), kSiblingCaveName);
    REQUIRE(peerSiblingAgain != nullptr);
    REQUIRE(peerSiblingAgain->tripCount() == 1);
    CHECK(peerSiblingAgain->trip(0)->name() == kSiblingTripName);
    CHECK(peerSiblingAgain->trip(0)->chunkCount() >= 1);
}

// ---------------------------------------------------------------------------
// Scenario F2 — the same delete/edit pair one level deeper: the peer deletes the
// whole Folder while the author edits a trip three levels down. Restoring only the
// chain to the edited trip would leave the Folder's other children behind, so the
// whole subtree comes back — every node and both note images.
// ---------------------------------------------------------------------------

TEST_CASE("A peer's delete of an ancestor loses to a local edit three levels down",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    auto clones = makeTwoClones(true, true);

    //--- The peer deletes the whole Folder and pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    cwSurveyNode* peerRoot = peerRegion->rootNode();
    peerRoot->removeNode(peerRoot->indexOfNode(peerFolder));
    pushPeer(clones.get());

    //--- The author edits the depth-3 section's trip, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorFolder = folderOf(authorRegion);
    cwCave* authorSideCave = childNamed(authorFolder, kSideCaveName);
    REQUIRE(authorSideCave != nullptr);
    cwCave* authorSection = childNamed(authorSideCave, kSectionName);
    REQUIRE(authorSection != nullptr);
    REQUIRE(authorSection->tripCount() == 1);

    const QDir dataRoot = clones->authorDataRoot();
    const QString folderDir = kFolderName;
    const QString siblingDir = folderDir + QStringLiteral("/nodes/") + kSiblingCaveName;
    const QString sectionDir =
        folderDir + QStringLiteral("/nodes/") + kSideCaveName + QStringLiteral("/nodes/") + kSectionName;
    const QByteArray originalSectionImage =
        noteImageBytes(QDir(dataRoot.absoluteFilePath(sectionDir)), kSectionTripName);
    const QByteArray originalSiblingImage =
        noteImageBytes(QDir(dataRoot.absoluteFilePath(siblingDir)), kSiblingTripName);

    SurveyTreeTestHelper::addShot(authorSection->trip(0), QStringLiteral("D1"), QStringLiteral("D2"));
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    //Every node under the deleted ancestor is still in the model, the untouched sibling
    //included: a floating survey would break every tie that resolved before the sync.
    cwCave* folderAfterSync = childNamed(authorRegion->rootNode(), kFolderName);
    REQUIRE(folderAfterSync != nullptr);
    cwCave* sideCaveAfterSync = childNamed(folderAfterSync, kSideCaveName);
    REQUIRE(sideCaveAfterSync != nullptr);
    CHECK(childNamed(sideCaveAfterSync, kSectionName) != nullptr);
    CHECK(childNamed(folderAfterSync, kSiblingCaveName) != nullptr);

    cwCave* sectionAfterSync = childNamed(sideCaveAfterSync, kSectionName);
    REQUIRE(sectionAfterSync != nullptr);
    REQUIRE(sectionAfterSync->tripCount() == 1);
    CHECK(sectionAfterSync->trip(0)->chunkCount() >= 1);

    //Both note images are back on disk with their bytes.
    const QStringList noteImages = filesWithSuffix(dataRoot, QStringLiteral(".png"));
    INFO("Note images: " << noteImages.join(QStringLiteral(", ")).toStdString());
    CHECK(noteImages.size() == 2);
    CHECK(noteImageBytes(QDir(dataRoot.absoluteFilePath(sectionDir)), kSectionTripName)
          == originalSectionImage);
    CHECK(noteImageBytes(QDir(dataRoot.absoluteFilePath(siblingDir)), kSiblingTripName)
          == originalSiblingImage);

    const QStringList fatalErrors = fatalLoadErrors(clones->authorProject()->filename());
    INFO("Fatal load errors: " << fatalErrors.join(QStringLiteral(" | ")).toStdString());
    CHECK(fatalErrors.isEmpty());
}

// ---------------------------------------------------------------------------
// Scenario F3 — the peer's delete with nothing of ours under it. Ours-wins has
// nothing to defend, so the delete stands: the node leaves the model and its trip
// is not adopted by another parent.
// ---------------------------------------------------------------------------

TEST_CASE("A peer's node delete stands when nothing local changed under it",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    static const QString kElsewhereTripName = QStringLiteral("Elsewhere survey");

    auto clones = makeTwoClones(true);

    //--- The peer deletes the depth-2 node and pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    cwCave* peerSibling = childNamed(peerFolder, kSiblingCaveName);
    REQUIRE(peerSibling != nullptr);
    peerFolder->removeNode(peerFolder->indexOfNode(peerSibling));
    pushPeer(clones.get());

    //--- The author's own work sits under a different node, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorSideCave = childNamed(folderOf(authorRegion), kSideCaveName);
    REQUIRE(authorSideCave != nullptr);
    SurveyTreeTestHelper::addTrip(authorSideCave, kElsewhereTripName, QStringLiteral("E"));
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    CHECK(childNamed(folderOf(authorRegion), kSiblingCaveName) == nullptr);

    const QList<cwTrip*> trips = authorRegion->rootNode()->allTrips();
    const bool deletedNodeTripAdopted = std::any_of(trips.begin(), trips.end(), [](const cwTrip* trip) {
        return trip != nullptr && trip->name() == kSiblingTripName;
    });
    CHECK_FALSE(deletedNodeTripAdopted);

    const bool authorTripSurvived = std::any_of(trips.begin(), trips.end(), [](const cwTrip* trip) {
        return trip != nullptr && trip->name() == kElsewhereTripName;
    });
    CHECK(authorTripSurvived);

    const QDir dataRoot = clones->authorDataRoot();
    const QStringList noteImages = filesWithSuffix(dataRoot, QStringLiteral(".png"));
    INFO("Note images: " << noteImages.join(QStringLiteral(", ")).toStdString());
    CHECK(noteImages.isEmpty());

    const QStringList fatalErrors = fatalLoadErrors(clones->authorProject()->filename());
    INFO("Fatal load errors: " << fatalErrors.join(QStringLiteral(" | ")).toStdString());
    CHECK(fatalErrors.isEmpty());
}

// ---------------------------------------------------------------------------
// Scenario F3b — the peer deletes the contested node and adds another one in the
// same push. The added descriptor belongs to no node of ours, so the handler asks
// for a full reload and its whole result is discarded — including the restore. The
// subtree must then stay as the merge left it: files written behind a discarded
// decision would sit uncommitted, and the next checkout refuses a dirty tree.
// ---------------------------------------------------------------------------

TEST_CASE("A discarded merge result leaves no half-restored files behind",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    static const QString kPeerAddedCaveName = QStringLiteral("Peer Added Cave");

    auto clones = makeTwoClones(true);

    //--- The peer deletes the depth-2 node and adds a new one, then pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerFolder = folderOf(peerRegion);
    cwCave* peerSibling = childNamed(peerFolder, kSiblingCaveName);
    REQUIRE(peerSibling != nullptr);
    peerFolder->removeNode(peerFolder->indexOfNode(peerSibling));
    addNode(peerRegion, peerFolder, cwSurveyNode::Kind::Cave, kPeerAddedCaveName);
    pushPeer(clones.get());

    //--- The author edits a trip inside the deleted node, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorSibling = childNamed(folderOf(authorRegion), kSiblingCaveName);
    REQUIRE(authorSibling != nullptr);
    REQUIRE(authorSibling->tripCount() == 1);
    SurveyTreeTestHelper::addShot(authorSibling->trip(0),
                                  QStringLiteral("S1"),
                                  QStringLiteral("S2"));
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    //Whatever the merge decides, disk and model agree about the node afterward: a
    //descriptor written back behind a discarded decision would describe a node no longer
    //in the tree, and the next load would resurrect it.
    const bool siblingInModel = childNamed(folderOf(authorRegion), kSiblingCaveName) != nullptr;
    const QDir siblingDir(clones->authorDataRoot().absoluteFilePath(
        kFolderName + QStringLiteral("/nodes/") + kSiblingCaveName));
    const bool siblingDescriptorOnDisk =
        !siblingDir.entryList({QStringLiteral("*.cwcave")}, QDir::Files).isEmpty();
    CHECK(siblingInModel == siblingDescriptorOnDisk);

    auto* authorRepository = clones->authorProject()->repository();
    REQUIRE(authorRepository != nullptr);
    authorRepository->checkStatus();
    CHECK(authorRepository->modifiedFileCount() == 0);

    const QStringList fatalErrors = fatalLoadErrors(clones->authorProject()->filename());
    INFO("Fatal load errors: " << fatalErrors.join(QStringLiteral(" | ")).toStdString());
    CHECK(fatalErrors.isEmpty());
}

// ---------------------------------------------------------------------------
// Scenario F4 — a hard reset back to a commit that predates a node. The target
// commit wins a reset, so ours-wins stands down: writing the node's subtree back
// would undo the reset and leave the working tree dirty for the next one.
// ---------------------------------------------------------------------------

TEST_CASE("A reset to a commit predating a node removes that node",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    static const QString kKeptCaveName = QStringLiteral("Kept Cave");
    static const QString kLaterCaveName = QStringLiteral("Later Cave");

    auto rootData = std::make_unique<cwRootData>();
    rootData->account()->setName(QStringLiteral("Author User"));
    rootData->account()->setEmail(QStringLiteral("author@example.com"));

    auto* project = rootData->project();
    auto* region = project->cavingRegion();
    region->setName(kProjectName);

    cwCave* keptCave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kKeptCaveName);
    SurveyTreeTestHelper::addTrip(keptCave, QStringLiteral("Kept survey"), QStringLiteral("K"));

    QTemporaryDir projectDir;
    REQUIRE(projectDir.isValid());
    const QString projectPath =
        QDir(projectDir.path()).filePath(kProjectName + QStringLiteral(".cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repository = project->repository();
    REQUIRE(repository != nullptr);
    const QString repoPath = repository->directory().absolutePath();
    const auto beforeNodeOid = QQuickGit::GitRepository::headCommitOid(repoPath);
    REQUIRE_FALSE(beforeNodeOid.hasError());

    //--- A node the target commit predates, with work of its own ---
    cwCave* laterCave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, kLaterCaveName);
    SurveyTreeTestHelper::addTrip(laterCave, QStringLiteral("Later survey"), QStringLiteral("L"));
    REQUIRE(project->save());
    project->waitSaveToFinish();

    const QDir dataRoot(QDir(repoPath).absoluteFilePath(kProjectName));
    REQUIRE(QFileInfo::exists(dataRoot.absoluteFilePath(kLaterCaveName)));

    project->errorModel()->clear();
    REQUIRE(project->resetBranchAndReconcile(beforeNodeOid.value()));
    project->waitForSyncToFinish();
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();
    CHECK(project->errorModel()->count() == 0);

    CHECK(childNamed(region->rootNode(), kKeptCaveName) != nullptr);
    CHECK(childNamed(region->rootNode(), kLaterCaveName) == nullptr);
    CHECK_FALSE(QFileInfo::exists(dataRoot.absoluteFilePath(kLaterCaveName)));

    //A restore written back behind the reset also blocks the next one: a checkout refuses
    //to run against a dirty working tree.
    repository->checkStatus();
    CHECK(repository->modifiedFileCount() == 0);
}

// ---------------------------------------------------------------------------
// Scenario G — A peer moves a node while the local side renames it. The move
// arrives as a directory that no longer sits where the live node says it does,
// and the rename writes the node's descriptor under a new name: the pair must
// still leave exactly one descriptor for that node and no duplicate row.
// ---------------------------------------------------------------------------

TEST_CASE("A peer's node move merges with a local rename of the same node",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    auto clones = makeTwoClones(false);

    //--- The peer moves Kentucky field seasons/nodes/Side Cave to the data root ---
    const QDir peerDataRoot(clones->cloneRepository.directory().absoluteFilePath(kProjectName));
    const QString movedFrom =
        peerDataRoot.absoluteFilePath(kFolderName + QStringLiteral("/nodes/") + kSideCaveName);
    const QString movedTo = peerDataRoot.absoluteFilePath(kSideCaveName);
    REQUIRE(QFileInfo::exists(movedFrom));
    REQUIRE(QDir().rename(movedFrom, movedTo));

    clones->cloneRepository.commitAll(QStringLiteral("Move Side Cave to the data root"), QString());
    auto pushFuture = clones->cloneRepository.push();
    REQUIRE(AsyncFuture::waitForFinished(pushFuture, 10000));
    INFO("Push error: " << pushFuture.result().errorMessage().toStdString());
    REQUIRE(!pushFuture.result().hasError());

    //--- The author renames the same node, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    QPointer<cwCave> authorFolder = folderOf(authorRegion);
    QPointer<cwCave> authorSideCave = childNamed(authorFolder, kSideCaveName);
    REQUIRE(authorSideCave != nullptr);
    const int nodeCountBefore = authorRegion->rootNode()->allNodes().size();

    authorSideCave->setName(kAuthorSideCave);
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    REQUIRE(authorSideCave != nullptr);
    CHECK(authorSideCave->parentNode() == authorFolder);
    CHECK(authorRegion->rootNode()->allNodes().size() == nodeCountBefore);

    //One descriptor per live node, and the renamed node's is the only one
    //carrying its name.
    const QDir dataRoot = clones->authorDataRoot();
    const QStringList descriptors = filesWithSuffix(dataRoot, QStringLiteral(".cwcave"));
    INFO("Descriptors: " << descriptors.join(QStringLiteral(", ")).toStdString());
    CHECK(descriptors.size() == nodeCountBefore);
    CHECK(descriptors.filter(kAuthorSideCave + QStringLiteral(".cwcave")).size() == 1);
    const QString oldDescriptorName = kSideCaveName + QStringLiteral(".cwcave");
    int oldNameDescriptors = 0;
    for (const QString& descriptor : descriptors) {
        if (QFileInfo(descriptor).fileName() == oldDescriptorName) {
            oldNameDescriptors++;
        }
    }
    CHECK(oldNameDescriptors == 0);

    const QStringList fatalErrors = fatalLoadErrors(clones->authorProject()->filename());
    INFO("Fatal load errors: " << fatalErrors.join(QStringLiteral(" | ")).toStdString());
    CHECK(fatalErrors.isEmpty());
}

// ---------------------------------------------------------------------------
// Scenario H — The depth-2 cases one level deeper: a rename/rename on a Section,
// and an ancestor rename concurrent with an edit to that Section's trip.
// ---------------------------------------------------------------------------

TEST_CASE("A depth-3 section survives a rename conflict and an ancestor rename",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    SECTION("rename/rename on the section itself")
    {
        auto clones = makeTwoClones(true, true);

        const QDir baseSectionDir(
            QDir(QFileInfo(clones->peerProject()->filename()).absolutePath())
                .absoluteFilePath(QStringLiteral("%1/%2/nodes/%3/nodes/%4")
                                      .arg(kProjectName, kFolderName, kSideCaveName, kSectionName)));
        const QByteArray baseImageBytes = noteImageBytes(baseSectionDir, kSectionTripName);

        auto* authorRegion = clones->authorProject()->cavingRegion();
        cwCave* authorSideCave = childNamed(folderOf(authorRegion), kSideCaveName);
        REQUIRE(authorSideCave != nullptr);
        cwCave* authorSection = childNamed(authorSideCave, kSectionName);
        REQUIRE(authorSection != nullptr);
        authorSection->setName(kAuthorSection);
        clones->authorProject()->waitSaveToFinish();
        syncAuthor(clones.get());

        auto* peerRegion = clones->peerProject()->cavingRegion();
        cwCave* peerSideCave = childNamed(folderOf(peerRegion), kSideCaveName);
        REQUIRE(peerSideCave != nullptr);
        QPointer<cwCave> peerSection = childNamed(peerSideCave, kSectionName);
        REQUIRE(peerSection != nullptr);
        peerSection->setName(kPeerSection);
        pushPeer(clones.get());

        REQUIRE(peerSection != nullptr);
        const QString winningName = peerSection->name();
        INFO("Winning section name: " << winningName.toStdString());
        CHECK((winningName == kPeerSection || winningName == kAuthorSection));
        CHECK(peerSection->parentNode() == peerSideCave);

        const QString losingName =
            winningName == kPeerSection ? kAuthorSection : kPeerSection;

        const QDir peerRepoRoot = QFileInfo(clones->peerProject()->filename()).absoluteDir();
        const QDir peerDataRoot(peerRepoRoot.absoluteFilePath(kProjectName));
        const QDir sectionParentNodesDir(peerDataRoot.absoluteFilePath(
            QStringLiteral("%1/nodes/%2/nodes").arg(kFolderName, kSideCaveName)));

        CHECK(QFileInfo::exists(sectionParentNodesDir.absoluteFilePath(
            QStringLiteral("%1/%1.cwcave").arg(winningName))));
        const QDir losingSectionDir(sectionParentNodesDir.absoluteFilePath(losingName));
        INFO("Left in the losing section directory: "
             << losingSectionDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)
                    .join(QStringLiteral(", ")).toStdString());
        CHECK_FALSE(QFileInfo::exists(sectionParentNodesDir.absoluteFilePath(losingName)));

        //The depth-3 trip's note image has exactly one copy, hydrated rather than left as
        //an LFS pointer by the winning directory's move.
        const QDir winningSectionDir(sectionParentNodesDir.absoluteFilePath(winningName));
        CHECK(noteImageFiles(winningSectionDir, kSectionTripName).size() == 1);
        CHECK(noteImageBytes(winningSectionDir, kSectionTripName) == baseImageBytes);

        //The losing directory goes away through the orphan cleanup and the rename job, both
        //of which only run when every handler stays on the deterministic merge path.
        const auto syncReport = clones->peerProject()->lastSyncReport();
        REQUIRE(syncReport.has_value());
        INFO("Diagnostics: " << syncReport->diagnostics.join(QStringLiteral(" | ")).toStdString());
        CHECK(handlerApplied(syncReport->diagnostics,
                             QStringLiteral("cwSurveyNodeSyncMergeHandler")));
        CHECK(handlerApplied(syncReport->diagnostics,
                             QStringLiteral("cwTripSyncMergeHandler")));
        CHECK_FALSE(fellBackToFullReload(syncReport->diagnostics));
    }

    SECTION("an ancestor rename concurrent with a depth-3 trip rename")
    {
        static const QString kPeerFolder = QStringLiteral("Peer field seasons");
        static const QString kAuthorTripName = QStringLiteral("Author dome climb");

        auto clones = makeTwoClones(true, true);

        auto* peerRegion = clones->peerProject()->cavingRegion();
        folderOf(peerRegion)->setName(kPeerFolder);
        pushPeer(clones.get());

        auto* authorRegion = clones->authorProject()->cavingRegion();
        QPointer<cwCave> authorFolder = folderOf(authorRegion);
        cwCave* authorSideCave = childNamed(authorFolder, kSideCaveName);
        REQUIRE(authorSideCave != nullptr);
        cwCave* authorSection = childNamed(authorSideCave, kSectionName);
        REQUIRE(authorSection != nullptr);
        REQUIRE(authorSection->tripCount() == 1);
        QPointer<cwTrip> authorTrip = authorSection->trip(0);
        authorTrip->setName(kAuthorTripName);
        clones->authorProject()->waitSaveToFinish();

        syncAuthor(clones.get());

        REQUIRE(authorFolder != nullptr);
        CHECK(authorFolder->name() == kPeerFolder);
        REQUIRE(authorTrip != nullptr);
        CHECK(authorTrip->name() == kAuthorTripName);

        const QDir dataRoot = clones->authorDataRoot();
        const QDir sectionDir(dataRoot.absoluteFilePath(
            QStringLiteral("%1/nodes/%2/nodes/%3").arg(kPeerFolder, kSideCaveName, kSectionName)));
        CHECK(sectionDir.exists());
        CHECK(noteImageFiles(sectionDir, kAuthorTripName).size() == 1);
    }
}

// ---------------------------------------------------------------------------
// Scenario I — Two peers add a same-named child under one parent. Both sides
// write a descriptor at the same path, so git merges one file: this case records
// what happens to the second node and its trip.
// ---------------------------------------------------------------------------

TEST_CASE("Two peers add a same-named child under one parent",
          "[cwSurveyNodeSyncMergeHandler][sync]")
{
    static const QString kAddedNodeName = QStringLiteral("New Cave");
    static const QString kPeerTripName = QStringLiteral("Peer added survey");
    static const QString kAuthorTripName = QStringLiteral("Author added survey");

    auto clones = makeTwoClones(true);

    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerAdded = SurveyTreeTestHelper::addNode(peerRegion, folderOf(peerRegion),
                                                      cwSurveyNode::Kind::Cave, kAddedNodeName);
    SurveyTreeTestHelper::addTrip(peerAdded, kPeerTripName, QStringLiteral("P"));
    pushPeer(clones.get());

    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorAdded = SurveyTreeTestHelper::addNode(authorRegion, folderOf(authorRegion),
                                                        cwSurveyNode::Kind::Cave, kAddedNodeName);
    SurveyTreeTestHelper::addTrip(authorAdded, kAuthorTripName, QStringLiteral("Q"));
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    //Both sides' survey data has to be somewhere in the tree.
    QStringList tripNames;
    const QList<cwTrip*> trips = authorRegion->rootNode()->allTrips();
    for (const cwTrip* trip : trips) {
        tripNames.append(trip->name());
    }
    INFO("Trips: " << tripNames.join(QStringLiteral(", ")).toStdString());
    CHECK(tripNames.contains(kAuthorTripName));
    CHECK(tripNames.contains(kPeerTripName));

    const QStringList fatalErrors = fatalLoadErrors(clones->authorProject()->filename());
    INFO("Fatal load errors: " << fatalErrors.join(QStringLiteral(" | ")).toStdString());
    CHECK(fatalErrors.isEmpty());
}

// ---------------------------------------------------------------------------
// Scenario J — Both sides edit the same note descriptor, each a different
// field: git resolves the .cwnote conflict to ours, so the peer's field only
// ever exists in the merged model. The note handler applies next to other
// handlers, and the merged descriptor has to reach disk in that same sync —
// otherwise the model and the repository disagree until some unrelated edit
// saves that note.
// ---------------------------------------------------------------------------

TEST_CASE("A merged note descriptor reaches disk when other handlers apply too",
          "[cwNoteSyncMergeHandler][sync]")
{
    static constexpr double kPeerNoteRotation = 37.0;
    static const QString kAuthorNoteName = QStringLiteral("Author note");

    auto clones = makeTwoClones(true, true);

    //--- The peer rotates the depth-3 trip's note and pushes ---
    auto* peerRegion = clones->peerProject()->cavingRegion();
    cwCave* peerSection = childNamed(childNamed(folderOf(peerRegion), kSideCaveName), kSectionName);
    REQUIRE(peerSection != nullptr);
    REQUIRE(peerSection->tripCount() == 1);
    const QList<cwNote*> peerNotes = peerSection->trip(0)->notes()->notes();
    REQUIRE(peerNotes.size() == 1);
    peerNotes.constFirst()->setRotate(kPeerNoteRotation);
    pushPeer(clones.get());

    //--- The author renames the same note, then syncs ---
    auto* authorRegion = clones->authorProject()->cavingRegion();
    cwCave* authorSection =
        childNamed(childNamed(folderOf(authorRegion), kSideCaveName), kSectionName);
    REQUIRE(authorSection != nullptr);
    REQUIRE(authorSection->tripCount() == 1);
    QList<cwNote*> authorNotes = authorSection->trip(0)->notes()->notes();
    REQUIRE(authorNotes.size() == 1);
    authorNotes.constFirst()->setName(kAuthorNoteName);
    clones->authorProject()->waitSaveToFinish();

    syncAuthor(clones.get());

    const auto syncReport = clones->authorProject()->lastSyncReport();
    REQUIRE(syncReport.has_value());
    INFO("Diagnostics: " << syncReport->diagnostics.join(QStringLiteral(" | ")).toStdString());
    CHECK(handlerApplied(syncReport->diagnostics, QStringLiteral("cwNoteSyncMergeHandler")));
    CHECK(handlerApplied(syncReport->diagnostics, QStringLiteral("cwNoteLiDARSyncMergeHandler")));
    CHECK_FALSE(fellBackToFullReload(syncReport->diagnostics));

    authorNotes = authorSection->trip(0)->notes()->notes();
    REQUIRE(authorNotes.size() == 1);
    CHECK(authorNotes.constFirst()->name() == kAuthorNoteName);
    CHECK(authorNotes.constFirst()->rotate() == Catch::Approx(kPeerNoteRotation));

    //The model took the peer's rotation on top of our name; so must the descriptor on disk.
    CHECK(reloadedNoteRotation(clones->authorProject()->filename(), kSectionTripName)
          == Catch::Approx(kPeerNoteRotation));
}
