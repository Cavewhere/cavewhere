// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwRootData.h"
#include "cwFutureManagerModel.h"
#include "cwErrorListModel.h"
#include "GitRepository.h"
#include "asyncfuture.h"
#include "ProjectFilenameTestHelper.h"

// Qt includes
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>
#include <QUrl>

// libgit2
#include "git2.h"

namespace {

static const QString kOriginalName = QStringLiteral("OriginalProject");
static const QString kRenamedName  = QStringLiteral("RenamedProject");

struct SyncRenameState {
    QTemporaryDir authorProjectDir;
    QTemporaryDir remoteRoot;
    QTemporaryDir cloneDir;

    std::unique_ptr<cwRootData> authorRootData;
    cwProject* authorProject = nullptr;

    // Paths captured BEFORE the author syncs
    QString originalDescriptorPath;
    QString originalDataRootPath;

    // Expected paths AFTER the author syncs
    QString expectedDescriptorPath;
    QString expectedDataRootPath;

    // Cave pointer captured before sync — still valid means no full reload
    QPointer<cwCave> localCavePtr;
};

// Perform the full sync rename scenario up to (and including) the author's sync.
// Returns false and leaves state partially filled if setup preconditions fail.
// Must be called from within a TEST_CASE so REQUIRE macros work correctly.
void setupSyncRenameScenario(SyncRenameState& s)
{
    REQUIRE(s.authorProjectDir.isValid());
    REQUIRE(s.remoteRoot.isValid());
    REQUIRE(s.cloneDir.isValid());

    // --- Author creates the original project ---
    s.authorRootData = std::make_unique<cwRootData>();
    s.authorProject = s.authorRootData->project();
    s.authorRootData->account()->setName(QStringLiteral("Author User"));
    s.authorRootData->account()->setEmail(QStringLiteral("author@example.com"));

    auto* region = s.authorProject->cavingRegion();
    region->setName(kOriginalName);
    region->addCave();
    auto* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Test Cave"));

    const QString projectPath =
        QDir(s.authorProjectDir.path()).filePath(kOriginalName + QStringLiteral(".cwproj"));
    REQUIRE(s.authorProject->saveAs(projectPath));
    s.authorProject->waitSaveToFinish();

    // --- Author pushes to bare remote ---
    auto* repository = s.authorProject->repository();
    REQUIRE(repository != nullptr);

    const QString remoteRepoPath =
        QDir(s.remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    REQUIRE(repository->addRemote(QStringLiteral("origin"),
                                  QUrl::fromLocalFile(remoteRepoPath)).isEmpty());

    s.authorProject->errorModel()->clear();
    REQUIRE(s.authorProject->sync());
    s.authorRootData->futureManagerModel()->waitForFinished();
    s.authorProject->waitSaveToFinish();
    CHECK(s.authorProject->errorModel()->count() == 0);

    // --- Peer clones the remote ---
    const QString clonePath = QDir(s.cloneDir.path()).filePath(QStringLiteral("peer-clone"));

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(clonePath));
    cloneRepository.setAccount(s.authorRootData->account());

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

    // Peer renames the project
    peerProject->cavingRegion()->setName(kRenamedName);
    peerProject->waitSaveToFinish();
    REQUIRE(peerProject->isModified());

    peerProject->errorModel()->clear();
    REQUIRE(peerProject->sync());
    peerRootData->futureManagerModel()->waitForFinished();
    peerProject->waitSaveToFinish();
    CHECK(peerProject->errorModel()->count() == 0);

    // --- Capture pre-sync state on author side ---
    // The .cwproj saveAs creates a git repo INSIDE a subdirectory named after the
    // project (e.g. <tempDir>/OriginalProject/), and the descriptor file lives at
    // <tempDir>/OriginalProject/OriginalProject.cwproj. Compute expected paths
    // relative to that git repo root.
    s.originalDescriptorPath = s.authorProject->filename();
    const QDir gitRepoRoot   = QFileInfo(s.originalDescriptorPath).absoluteDir();
    s.originalDataRootPath   = gitRepoRoot.absoluteFilePath(kOriginalName);
    s.expectedDescriptorPath = gitRepoRoot.absoluteFilePath(kRenamedName + QStringLiteral(".cwproj"));
    s.expectedDataRootPath   = gitRepoRoot.absoluteFilePath(kRenamedName);
    s.localCavePtr           = s.authorProject->cavingRegion()->cave(0);
    REQUIRE(s.localCavePtr != nullptr);

    // --- Author syncs to pull rename ---
    s.authorProject->errorModel()->clear();
    REQUIRE(s.authorProject->sync());
    s.authorRootData->futureManagerModel()->waitForFinished();
    s.authorProject->waitSaveToFinish();
    CHECK(s.authorProject->errorModel()->count() == 0);

}

} // namespace

TEST_CASE("Reconcile handler applies project name change without full reload",
          "[cwCavingRegionSyncMergeHandler][sync]")
{
    SyncRenameState s;
    setupSyncRenameScenario(s);

    // The in-memory region name must reflect the remote rename.
    CHECK(s.authorProject->cavingRegion()->name() == kRenamedName);

    // The original cwCave object must still be alive — full reload would have
    // destroyed it and replaced it with a new instance.
    CHECK(s.localCavePtr != nullptr);
}

TEST_CASE("Reconcile handler renames dataRoot directory on disk when name changes",
          "[cwCavingRegionSyncMergeHandler][sync]")
{
    SyncRenameState s;
    setupSyncRenameScenario(s);

    CHECK(QFileInfo::exists(s.expectedDataRootPath));
    CHECK(!QFileInfo::exists(s.originalDataRootPath));
}

TEST_CASE("Reconcile handler renames the .cwproj descriptor file on disk when name changes",
          "[cwCavingRegionSyncMergeHandler][sync]")
{
    SyncRenameState s;
    setupSyncRenameScenario(s);

    CHECK(QFileInfo::exists(s.expectedDescriptorPath));
    CHECK(!QFileInfo::exists(s.originalDescriptorPath));
}

TEST_CASE("dataRoot metadata is updated to the new sanitized name after sync rename",
          "[cwCavingRegionSyncMergeHandler][sync]")
{
    SyncRenameState s;
    setupSyncRenameScenario(s);

    // cwProject::dataRoot() returns the sanitized directory name (not a full path).
    CHECK(s.authorProject->dataRoot() == kRenamedName);
    // cwProject::filename() must reflect the new descriptor path.
    CHECK(s.authorProject->filename() == s.expectedDescriptorPath);
}

TEST_CASE("Object paths inside the renamed dataRoot remain valid after the rename",
          "[cwCavingRegionSyncMergeHandler][sync]")
{
    SyncRenameState s;
    setupSyncRenameScenario(s);

    REQUIRE(s.localCavePtr != nullptr);

    // The cave's file on disk should exist under the new dataRoot.
    const QString caveFilePath = ProjectFilenameTestHelper::absolutePath(s.localCavePtr);
    INFO("Cave file path: " << caveFilePath.toStdString());
    CHECK(QFileInfo::exists(caveFilePath));

    // The cave file path must be rooted inside the new dataRoot, not the old one.
    CHECK(caveFilePath.startsWith(s.expectedDataRootPath));
    CHECK(!caveFilePath.startsWith(s.originalDataRootPath));
}

// ---------------------------------------------------------------------------
// Concurrent-conflict test: both author and peer rename from the same base.
// CaveWhere's "ours-wins" git conflict resolution keeps the author's rename,
// so the local name must survive the sync.
// ---------------------------------------------------------------------------

TEST_CASE("Local project rename wins on concurrent conflict",
          "[cwCavingRegionSyncMergeHandler][sync]")
{
    static const QString kAuthorRename = QStringLiteral("AuthorRenamedProject");
    static const QString kPeerRename   = QStringLiteral("PeerRenamedProject");

    // --- Author creates the initial project ---
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
    region->setName(kOriginalName);
    region->addCave();
    REQUIRE(region->cave(0) != nullptr);
    region->cave(0)->setName(QStringLiteral("Conflict Cave"));

    const QString projectPath =
        QDir(authorProjectDir.path()).filePath(kOriginalName + QStringLiteral(".cwproj"));
    REQUIRE(authorProject->saveAs(projectPath));
    authorProject->waitSaveToFinish();

    // --- Author pushes to bare remote ---
    auto* repository = authorProject->repository();
    REQUIRE(repository != nullptr);

    const QString remoteRepoPath =
        QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remoteRepoPath.toLocal8Bit().constData(), 1) == GIT_OK);
    if (remoteRepo) {
        git_repository_free(remoteRepo);
        remoteRepo = nullptr;
    }

    REQUIRE(repository->addRemote(QStringLiteral("origin"),
                                  QUrl::fromLocalFile(remoteRepoPath)).isEmpty());

    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();
    CHECK(authorProject->errorModel()->count() == 0);

    // --- Peer clones and renames to kPeerRename, then pushes ---
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

    peerProject->cavingRegion()->setName(kPeerRename);
    peerProject->waitSaveToFinish();
    REQUIRE(peerProject->isModified());

    peerProject->errorModel()->clear();
    REQUIRE(peerProject->sync());
    peerRootData->futureManagerModel()->waitForFinished();
    peerProject->waitSaveToFinish();
    CHECK(peerProject->errorModel()->count() == 0);

    // --- Author renames locally to kAuthorRename WITHOUT syncing yet ---
    // This creates a rename/rename divergence: peer has kPeerRename on the
    // remote, author has kAuthorRename locally. When the author syncs, git
    // resolves the conflict with "ours wins", keeping kAuthorRename.
    authorProject->cavingRegion()->setName(kAuthorRename);
    authorProject->waitSaveToFinish();
    REQUIRE(authorProject->isModified());

    // --- Author syncs: commits local rename, pulls peer's rename, ours wins ---
    authorProject->errorModel()->clear();
    REQUIRE(authorProject->sync());
    authorRootData->futureManagerModel()->waitForFinished();
    authorProject->waitSaveToFinish();

    // The sync must complete without error.
    CHECK(authorProject->errorModel()->count() == 0);

    // The author's rename must survive: local wins on concurrent conflict.
    CHECK(authorProject->cavingRegion()->name() == kAuthorRename);
}
