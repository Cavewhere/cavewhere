// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

// Std
#include <algorithm>

// Ours
#include "cwProjectSyncHealth.h"
#include "GitRepository.h"
#include "Account.h"
#include "asyncfuture.h"
#include "TestHelper.h"
#include "ProfileLogCapture.h"
#include "MainThreadStallHelpers.h"

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

constexpr qint64 kStaleStatOffsetSeconds = -3600;

QString syntheticFileName(int index)
{
    return QStringLiteral("f%1.txt").arg(index);
}

// Commits through a repository that is gone before the test starts, so the
// repository under test has no status walk in flight.
void commitSyntheticWorkTree(const QString& path, int fileCount)
{
    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    GitRepository setup;
    setup.setDirectory(QDir(path));
    setup.initRepository();
    setup.setAccount(&account);
    for (int i = 0; i < fileCount; ++i) {
        writeFile(path, syntheticFileName(i), QStringLiteral("line %1\n").arg(i));
    }
    CHECK_NOTHROW(setup.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));
}

// Moves every committed file's mtime to a whole second an hour back, so the
// index stat cache disagrees and the next status walk re-hashes each file.
void makeStatDataStale(const QString& path, int fileCount)
{
    const QDateTime past =
        QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch() + kStaleStatOffsetSeconds);
    for (int i = 0; i < fileCount; ++i) {
        QFile file(QDir(path).filePath(syntheticFileName(i)));
        REQUIRE(file.open(QFile::ReadWrite));
        REQUIRE(file.setFileTime(past, QFileDevice::FileModificationTime));
    }
}

struct RecomposeMeasurement
{
    qint64 longestStallMs = 0;
    qsizetype gitStatusLines = 0;
    qsizetype modifiedCountChanges = 0;
};

// Settles a sync health on a clean work tree of fileCount files, then adds one
// file and refreshes the count through the repository alone.
RecomposeMeasurement measureRecompose(int fileCount)
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString path = tempDir.path();
    commitSyntheticWorkTree(path, fileCount);

    GitRepository repo;
    repo.setDirectory(QDir(path));
    repo.initRepository();

    cwProjectSyncHealth syncHealth;
    syncHealth.setRepository(&repo);
    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote;
    }));
    REQUIRE(!syncHealth.status().hasLocalChanges());

    QSignalSpy modifiedSpy(&repo, &GitRepository::modifiedFileCountChanged);
    const ProfileLogCapture capture(QStringLiteral("cw.profile.load.debug=true"));

    RecomposeMeasurement measurement;
    measurement.longestStallMs = longestMainThreadStallUntil(
        [&repo, &path]() {
            writeFile(path, QStringLiteral("added.txt"), QStringLiteral("added\n"));
            repo.checkStatusAsync();
        },
        [&syncHealth]() { return syncHealth.status().hasLocalChanges(); });
    measurement.gitStatusLines =
        ProfileLogCapture::linesStartingWith(QStringLiteral("project phase=gitStatus ")).size();
    measurement.modifiedCountChanges = modifiedSpy.count();
    return measurement;
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
    REQUIRE(initBareRepo(remotePath) == GIT_OK);

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

    REQUIRE(initBareRepo(remotePath) == GIT_OK);

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
    REQUIRE(initBareRepo(remotePath) == GIT_OK);

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

TEST_CASE("cwProjectSyncHealth writes a cw.profile.load line for each status walk", "[cwProjectSyncHealth]")
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
    writeFile(tempDir.path(), QStringLiteral("added.txt"), QStringLiteral("untracked\n"));

    const ProfileLogCapture capture(QStringLiteral("cw.profile.load.debug=true"));

    cwProjectSyncHealth syncStatus;
    syncStatus.setRepository(&repo);
    syncStatus.refresh();

    const QString prefix = QStringLiteral("project phase=gitStatus ");
    REQUIRE(waitUntil([&prefix]() {
        return !ProfileLogCapture::linesStartingWith(prefix).isEmpty();
    }));

    const QRegularExpression pattern(QStringLiteral("^project phase=gitStatus us=\\d+ modified=1$"));
    const QStringList lines = ProfileLogCapture::linesStartingWith(prefix);
    for (const QString& line : lines) {
        INFO("line: " << line.toStdString());
        CHECK(pattern.match(line).hasMatch());
    }
}

TEST_CASE("cwProjectSyncHealth refresh returns before the status lands", "[cwProjectSyncHealth]")
{
    constexpr int kFileCount = 32;

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    commitSyntheticWorkTree(tempDir.path(), kFileCount);
    makeStatDataStale(tempDir.path(), kFileCount);

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwProjectSyncHealth syncHealth;
    QSignalSpy statusSpy(&syncHealth, &cwProjectSyncHealth::statusChanged);
    syncHealth.setRepository(&repo);
    syncHealth.refresh();

    CHECK(statusSpy.count() == 0);

    REQUIRE(waitUntil([&statusSpy]() { return statusSpy.count() >= 1; }));
    CHECK(syncHealth.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote);
    CHECK(!syncHealth.status().hasLocalChanges());
}

TEST_CASE("cwProjectSyncHealth runs no status walk inside initRepository and bounds the emissions",
          "[cwProjectSyncHealth]")
{
    constexpr int kFileCount = 8;
    constexpr qsizetype kMaxStatusChangesPerOpen = 2;

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    commitSyntheticWorkTree(tempDir.path(), kFileCount);
    makeStatDataStale(tempDir.path(), kFileCount);
    writeFile(tempDir.path(), QStringLiteral("untracked.txt"), QStringLiteral("untracked\n"));

    // As in cwProject: the sync health holds the repository before the load opens it.
    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));

    cwProjectSyncHealth syncHealth;
    syncHealth.setRepository(&repo);
    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote;
    }));

    QSignalSpy modifiedSpy(&repo, &GitRepository::modifiedFileCountChanged);
    QSignalSpy statusSpy(&syncHealth, &cwProjectSyncHealth::statusChanged);

    repo.initRepository();

    CHECK(modifiedSpy.count() == 0);

    REQUIRE(waitUntil([&syncHealth]() { return syncHealth.status().hasLocalChanges(); }));
    REQUIRE(AsyncFuture::waitForFinished(repo.checkStatusAsync(), DefaultTimeoutMs));

    CHECK(repo.modifiedFileCount() == 1);
    CHECK(modifiedSpy.count() == 1);
    CHECK(statusSpy.count() <= kMaxStatusChangesPerOpen);
    CHECK(syncHealth.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote);
}

TEST_CASE("cwProjectSyncHealth recomposes on a count change without another status walk",
          "[cwProjectSyncHealth]")
{
    constexpr int kBaseFileCount = 64;
    constexpr int kGrowthFactor = 4;
    constexpr qint64 kGrowthRatio = 4;

    const RecomposeMeasurement base = measureRecompose(kBaseFileCount);
    const RecomposeMeasurement grown = measureRecompose(kBaseFileCount * kGrowthFactor);

    CHECK(base.gitStatusLines == 0);
    CHECK(grown.gitStatusLines == 0);
    CHECK(base.modifiedCountChanges == 1);
    CHECK(grown.modifiedCountChanges == 1);

    INFO("base stall ms: " << base.longestStallMs << " grown stall ms: " << grown.longestStallMs);
    CHECK(grown.longestStallMs
          <= kGrowthRatio * std::max(base.longestStallMs, qint64(mainThreadStall::kFrameIntervalMs)));
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
