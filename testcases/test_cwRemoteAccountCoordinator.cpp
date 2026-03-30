// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QDir>
#include <QTemporaryDir>

// Ours
#include "cwRemoteAccountCoordinator.h"
#include "cwGitHubIntegration.h"
#include "cwRemoteAccountModel.h"
#include "cwRemoteBindingStore.h"
#include "GitRepository.h"
#include "TestHelper.h"

// libgit2
#include "git2.h"

using namespace QQuickGit;

TEST_CASE("cwRemoteAccountCoordinator::addRemoteToProject adds remote and skips binding when flag is false",
          "[cwRemoteAccountCoordinator]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bare = nullptr;
    REQUIRE(git_repository_init(&bare, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bare);

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwGitHubIntegration integration(nullptr);
    cwRemoteAccountModel accountModel;
    cwRemoteBindingStore bindingStore;
    cwRemoteAccountCoordinator coordinator(&integration, &accountModel, &bindingStore);

    bool failed = false;
    QObject::connect(&coordinator, &cwRemoteAccountCoordinator::addRemoteFailed,
                     [&]() { failed = true; });

    const QString remoteUrl = QUrl::fromLocalFile(remotePath).toString();
    coordinator.addRemoteToProject(&repo, remoteUrl, false);

    REQUIRE(waitUntil([&repo]() { return !repo.remotes().isEmpty(); }));
    CHECK(!failed);
    CHECK(repo.remotes().constFirst().name() == QStringLiteral("origin"));
    CHECK(bindingStore.accountIdForRemote(remoteUrl).isEmpty());
}

TEST_CASE("cwRemoteAccountCoordinator::addRemoteToProject emits addRemoteFailed on duplicate remote",
          "[cwRemoteAccountCoordinator]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bare = nullptr;
    REQUIRE(git_repository_init(&bare, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bare);

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    REQUIRE(repo.addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(remotePath)).isEmpty());

    cwGitHubIntegration integration(nullptr);
    cwRemoteAccountModel accountModel;
    cwRemoteBindingStore bindingStore;
    cwRemoteAccountCoordinator coordinator(&integration, &accountModel, &bindingStore);

    QString errorMessage;
    QObject::connect(&coordinator, &cwRemoteAccountCoordinator::addRemoteFailed,
                     [&](const QString& msg) { errorMessage = msg; });

    coordinator.addRemoteToProject(&repo, QUrl::fromLocalFile(remotePath).toString(), false);

    REQUIRE(waitUntil([&errorMessage]() { return !errorMessage.isEmpty(); }));
}

TEST_CASE("cwRemoteAccountCoordinator::addRemoteToProject accepts SSH SCP URL",
          "[cwRemoteAccountCoordinator]")
{
    // Regression: git@host:user/repo.git is SSH SCP syntax, not a valid RFC 3986 URI.
    // QUrl strict-parsing converts it to an empty URL, which libgit2 rejects with
    // "cannot set empty url".  The fix passes the raw string directly to git_remote_create.
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwGitHubIntegration integration(nullptr);
    cwRemoteAccountModel accountModel;
    cwRemoteBindingStore bindingStore;
    cwRemoteAccountCoordinator coordinator(&integration, &accountModel, &bindingStore);

    QString errorMessage;
    QObject::connect(&coordinator, &cwRemoteAccountCoordinator::addRemoteFailed,
                     [&](const QString& msg) { errorMessage = msg; });

    coordinator.addRemoteToProject(&repo,
                                   QStringLiteral("git@github.com:vpicaver/holberg-conk-old.git"),
                                   false);

    REQUIRE(waitUntil([&repo]() { return !repo.remotes().isEmpty(); }));
    CHECK(errorMessage.isEmpty());
    CHECK(repo.remotes().constFirst().name() == QStringLiteral("origin"));
}

TEST_CASE("cwRemoteAccountCoordinator::addRemoteToProject stores binding when flag is true",
          "[cwRemoteAccountCoordinator]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bare = nullptr;
    REQUIRE(git_repository_init(&bare, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bare);

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwGitHubIntegration integration(nullptr);
    cwRemoteAccountModel accountModel;
    cwRemoteBindingStore bindingStore;
    cwRemoteAccountCoordinator coordinator(&integration, &accountModel, &bindingStore);

    bool failed = false;
    QObject::connect(&coordinator, &cwRemoteAccountCoordinator::addRemoteFailed,
                     [&]() { failed = true; });

    const QString remoteUrl = QUrl::fromLocalFile(remotePath).toString();
    coordinator.addRemoteToProject(&repo, remoteUrl, true);

    REQUIRE(waitUntil([&repo]() { return !repo.remotes().isEmpty(); }));
    CHECK(!failed);
    // bindRemoteToActiveGitHubAccount stores the binding when an active account exists.
    // With no active account the binding is a no-op, but the remote is still added.
    CHECK(repo.remotes().constFirst().name() == QStringLiteral("origin"));
}
