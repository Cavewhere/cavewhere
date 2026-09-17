// Catch2 includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "LoadProjectHelper.h"
#include "SurveyTreeSyncFixture.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwErrorListModel.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwTrip.h"
#include "GitRepository.h"
#include "asyncfuture.h"

// Qt includes
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>
#include <QUrl>

#include <QDirIterator>

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
