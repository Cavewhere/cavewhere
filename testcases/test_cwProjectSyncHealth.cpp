// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

// Ours
#include "cwProjectSyncHealth.h"
#include "GitRepository.h"
#include "Account.h"
#include "asyncfuture.h"
#include "TestHelper.h"

// libgit2
#include "git2.h"

using namespace QQuickGit;

namespace {
constexpr int DefaultTimeoutMs = 10000;

void waitForGitFuture(const GitRepository::GitFuture& future, int timeout = DefaultTimeoutMs)
{
    REQUIRE(AsyncFuture::waitForFinished(future, timeout));
    INFO("Git error:" << future.result().errorMessage().toStdString()
                      << " code:" << future.result().errorCode());
    REQUIRE(!future.result().hasError());
}

void writeFile(const QString& directoryPath, const QString& fileName, const QString& contents)
{
    QFile file(QDir(directoryPath).filePath(fileName));
    REQUIRE(file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text));
    file.write(contents.toUtf8());
}
} // namespace

TEST_CASE("cwProjectSyncHealth reports repository unavailable warning", "[cwProjectSyncHealth]")
{
    cwProjectSyncHealth syncStatus;
    syncStatus.setRepository(nullptr);
    syncStatus.refresh();

    CHECK(syncStatus.status().stale());
    CHECK(syncStatus.status().message() == QStringLiteral("Sync unavailable: repository is not initialized."));
    CHECK(syncStatus.status().aheadCount() == 0);
    CHECK(syncStatus.status().behindCount() == 0);
}

TEST_CASE("cwProjectSyncHealth reports missing remote warning", "[cwProjectSyncHealth]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    repo.setAccount(&account);

    writeFile(tempDir.path(), QStringLiteral("state.txt"), QStringLiteral("initial\n"));
    CHECK_NOTHROW(repo.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));

    cwProjectSyncHealth syncStatus;
    syncStatus.setRepository(&repo);
    syncStatus.refresh();

    REQUIRE(waitUntil([&syncStatus]() {
        return syncStatus.status().message() == QStringLiteral("No git remote is configured for this project.");
    }));
    CHECK(syncStatus.status().stale());
    CHECK(syncStatus.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote);
    CHECK(!syncStatus.status().hasRemote());
    CHECK(!syncStatus.status().needsLogin());
    CHECK(!syncStatus.status().authExpired());
}

TEST_CASE("cwProjectSyncHealth prefers missing remote warning for new repository", "[cwProjectSyncHealth]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwProjectSyncHealth syncStatus;
    syncStatus.setRepository(&repo);
    syncStatus.refresh();

    REQUIRE(waitUntil([&syncStatus]() {
        return syncStatus.status().message() == QStringLiteral("No git remote is configured for this project.");
    }));
    CHECK(syncStatus.status().stale());
    CHECK(syncStatus.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote);
    CHECK(!syncStatus.status().hasRemote());
}

TEST_CASE("cwProjectSyncHealth updates status after initRepository on repo with existing remote",
          "[cwProjectSyncHealth]")
{
    // Regression: cwProject assigns the GitRepository pointer to cwProjectSyncHealth at startup,
    // before the file is loaded and initRepository() is called. Because initRepository() does not
    // emit remotesChanged (or headBranchNameChanged), refresh() is never re-triggered, and the
    // SyncButton keeps showing "No git remote is configured for this project." even though the
    // on-disk repo has a remote configured.

    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("project"));

    // --- Phase 1: build a real on-disk git repo that has an origin remote ---
    git_repository* bareRepo = nullptr;
    REQUIRE(git_repository_init(&bareRepo, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bareRepo);

    REQUIRE(QDir().mkpath(projectPath));

    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    {
        GitRepository setup;
        setup.setDirectory(QDir(projectPath));
        setup.initRepository();
        setup.setAccount(&account);
        setup.addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(remotePath));
        writeFile(projectPath, QStringLiteral("data.txt"), QStringLiteral("initial\n"));
        CHECK_NOTHROW(setup.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));
        waitForGitFuture(setup.push());
    }
    // setup is destroyed; the on-disk repo now has a remote and at least one pushed commit

    // --- Phase 2: simulate cwProject startup — attach syncHealth before opening the file ---
    // This mirrors the real app: connectSaveLoad() calls syncHealth->setRepository(repo) before
    // any file is loaded, so the GitRepository has a directory set but initRepository() has not
    // been called yet (d->repo == nullptr, remotes() returns empty).
    GitRepository repo;
    repo.setDirectory(QDir(projectPath));
    // initRepository() deliberately NOT called yet

    cwProjectSyncHealth syncHealth;
    syncHealth.setRepository(&repo);

    // Confirm the buggy initial state: no libgit2 handle → remotes() is empty → stale message
    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().message()
               == QStringLiteral("No git remote is configured for this project.");
    }));

    // --- Phase 3: simulate cwSaveLoad::initializeRepositoryForCurrentFile() ---
    // This is what the load path calls after setFileName(). It opens the libgit2 handle and
    // makes remotes() return real results. For the fix to work, initRepository() must emit
    // remotesChanged() so that cwProjectSyncHealth::refresh() is re-triggered.
    repo.initRepository();

    // The status must change away from "No git remote is configured for this project."
    // (it will become an ahead/behind result or a stale-but-different error — anything except
    // the "no remote" message confirms that refresh() ran with the initialized repo)
    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().message()
               != QStringLiteral("No git remote is configured for this project.");
    }));
}

TEST_CASE("cwProjectSyncHealth resolves remote ahead/behind asynchronously", "[cwProjectSyncHealth]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    const QString authorPath = QDir(tempDir.path()).filePath(QStringLiteral("author"));
    const QString peerPath = QDir(tempDir.path()).filePath(QStringLiteral("peer"));

    git_repository* remoteRepo = nullptr;
    REQUIRE(git_repository_init(&remoteRepo, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    REQUIRE(remoteRepo != nullptr);
    git_repository_free(remoteRepo);

    REQUIRE(QDir().mkpath(authorPath));

    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    GitRepository author;
    author.setDirectory(QDir(authorPath));
    author.initRepository();
    author.setAccount(&account);
    author.addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(remotePath));

    writeFile(authorPath, QStringLiteral("state.txt"), QStringLiteral("author-initial\n"));
    CHECK_NOTHROW(author.commitAll(QStringLiteral("Initial"), QStringLiteral("author initial commit")));
    waitForGitFuture(author.push());

    GitRepository peer;
    peer.setDirectory(QDir(peerPath));
    peer.setAccount(&account);
    waitForGitFuture(peer.clone(QUrl::fromLocalFile(remotePath)));

    writeFile(peerPath, QStringLiteral("state.txt"), QStringLiteral("peer-advance\n"));
    CHECK_NOTHROW(peer.commitAll(QStringLiteral("Peer"), QStringLiteral("advance remote tip")));
    waitForGitFuture(peer.push());

    cwProjectSyncHealth syncStatus;
    syncStatus.setRepository(&author);
    syncStatus.refresh();

    REQUIRE(waitUntil([&syncStatus]() {
        return !syncStatus.status().stale();
    }, 6000));
    CHECK(syncStatus.status().behindCount() >= 1);
}

TEST_CASE("cwProjectSyncHealth sets SyncBlocker::NoRemote when no remote configured", "[cwProjectSyncHealth]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    repo.setAccount(&account);

    writeFile(tempDir.path(), QStringLiteral("state.txt"), QStringLiteral("initial\n"));
    CHECK_NOTHROW(repo.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));

    cwProjectSyncHealth syncHealth;
    syncHealth.setRepository(&repo);
    syncHealth.refresh();

    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote;
    }));
    CHECK(!syncHealth.status().hasRemote());
    CHECK(!syncHealth.status().needsLogin());
    CHECK(!syncHealth.status().authExpired());
}

TEST_CASE("cwProjectSyncHealth clears NoRemote after remote is added", "[cwProjectSyncHealth]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bareRepo = nullptr;
    REQUIRE(git_repository_init(&bareRepo, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bareRepo);

    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    repo.setAccount(&account);

    writeFile(tempDir.path(), QStringLiteral("state.txt"), QStringLiteral("initial\n"));
    CHECK_NOTHROW(repo.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));

    cwProjectSyncHealth syncHealth;
    syncHealth.setRepository(&repo);
    syncHealth.refresh();

    REQUIRE(waitUntil([&syncHealth]() { return !syncHealth.status().hasRemote(); }));

    REQUIRE(repo.addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(remotePath)).isEmpty());

    REQUIRE(waitUntil([&syncHealth]() { return syncHealth.status().hasRemote(); }));
    CHECK(syncHealth.status().syncBlocker() != cwSyncStatus::SyncBlocker::NoRemote);
}

TEST_CASE("cwSyncStatus operator== compares syncBlocker", "[cwSyncStatus]")
{
    cwSyncStatus a, b;
    CHECK(a == b);

    a.m_syncBlocker = cwSyncStatus::SyncBlocker::NoRemote;
    CHECK(a != b);

    b.m_syncBlocker = cwSyncStatus::SyncBlocker::NoRemote;
    CHECK(a == b);
}
