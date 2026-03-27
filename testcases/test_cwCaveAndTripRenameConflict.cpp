// Catch2 includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
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

// libgit2
#include "git2.h"

// ---------------------------------------------------------------------------
// Scenario A — Cave rename/rename conflict
//
// base:   one cave named "Conflict Cave"
// local:  cave renamed → "Author Cave"   (committed, not yet pushed)
// remote: cave renamed → "Peer Cave"     (peer pushed)
// action: local syncs → ours wins
//
// Known gap: the "Peer Cave" directory is currently left on disk after the
// merge because cwCaveSyncMergeHandler has no orphan-cleanup step.
// The CHECK_FALSE assertions below document the gap and will fail until the
// fix is implemented.
// ---------------------------------------------------------------------------

TEST_CASE("Local cave rename wins on concurrent rename-rename conflict",
          "[cwCaveSyncMergeHandler][sync]")
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

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

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
    REQUIRE(peerProject->isModified());

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
    REQUIRE(authorProject->isModified());

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
    // These assertions document the known gap in cwCaveSyncMergeHandler and
    // are expected to fail until the orphan-cleanup step is added.
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
// action: local syncs → ours wins
//
// Known gap: the "Peer Trip" directory is currently left on disk after the
// merge because cwTripSyncMergeHandler has no orphan-cleanup step.
// The CHECK_FALSE assertions below document the gap and will fail until the
// fix is implemented.
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

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

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
    REQUIRE(peerProject->isModified());

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
    REQUIRE(authorProject->isModified());

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
    // These assertions document the known gap in cwTripSyncMergeHandler and
    // are expected to fail until the orphan-cleanup step is added.
    CHECK_FALSE(QFileInfo::exists(tripsDir.absoluteFilePath(kPeerTrip)));
    CHECK_FALSE(QFileInfo::exists(
        tripsDir.absoluteFilePath(kPeerTrip + QChar('/') + kPeerTrip + QStringLiteral(".cwtrip"))));
}
