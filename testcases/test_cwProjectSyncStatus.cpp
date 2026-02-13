// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <functional>

// Ours
#include "cwProjectSyncStatus.h"
#include "GitRepository.h"
#include "Account.h"
#include "asyncfuture.h"

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

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return condition();
}
} // namespace

TEST_CASE("cwProjectSyncStatus reports repository unavailable warning", "[cwProjectSyncStatus]")
{
    cwProjectSyncStatus syncStatus;
    syncStatus.setRepository(nullptr);
    syncStatus.refresh();

    CHECK(syncStatus.status().stale());
    CHECK(syncStatus.status().message() == QStringLiteral("Sync unavailable: repository is not initialized."));
    CHECK(syncStatus.status().aheadCount() == 0);
    CHECK(syncStatus.status().behindCount() == 0);
}

TEST_CASE("cwProjectSyncStatus reports missing remote warning", "[cwProjectSyncStatus]")
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

    cwProjectSyncStatus syncStatus;
    syncStatus.setRepository(&repo);
    syncStatus.refresh();

    REQUIRE(waitUntil([&syncStatus]() {
        return syncStatus.status().message() == QStringLiteral("No git remote is configured for this project.");
    }));
    CHECK(syncStatus.status().stale());
}

TEST_CASE("cwProjectSyncStatus prefers missing remote warning for new repository", "[cwProjectSyncStatus]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwProjectSyncStatus syncStatus;
    syncStatus.setRepository(&repo);
    syncStatus.refresh();

    REQUIRE(waitUntil([&syncStatus]() {
        return syncStatus.status().message() == QStringLiteral("No git remote is configured for this project.");
    }));
    CHECK(syncStatus.status().stale());
}

TEST_CASE("cwProjectSyncStatus resolves remote ahead/behind asynchronously", "[cwProjectSyncStatus]")
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

    cwProjectSyncStatus syncStatus;
    syncStatus.setRepository(&author);
    syncStatus.refresh();

    REQUIRE(waitUntil([&syncStatus]() {
        return !syncStatus.status().stale();
    }, 6000));
    CHECK(syncStatus.status().behindCount() >= 1);
}
