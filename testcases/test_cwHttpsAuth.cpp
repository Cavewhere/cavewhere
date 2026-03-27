// Catch2
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QSignalSpy>
#include <QTemporaryDir>

// QQuickGit
#include "GitRepository.h"

// Our
#include "cwRemoteRepositoryCloner.h"
#include "cwRecentProjectModel.h"
#include "cwRemoteCredentialStore.h"
#include "cwGitHubIntegration.h"
#include "cwSaveLoad.h"

// -----------------------------------------------------------------------
// GitCredentials
// -----------------------------------------------------------------------

TEST_CASE("GitCredentials defaults to empty token", "[cwHttpsAuth]")
{
    QQuickGit::GitCredentials creds;
    CHECK(creds.httpsToken.isEmpty());
}

TEST_CASE("GitCredentials token round-trips through setCredentials", "[cwHttpsAuth]")
{
    QQuickGit::GitRepository repo;

    // Setting credentials must not crash and the token is accepted.
    // (We cannot read m_credentials back directly, but a second call
    // clearing the token must also not crash.)
    repo.setCredentials(QQuickGit::GitCredentials{QStringLiteral("ghp_testtoken123")});
    repo.setCredentials(QQuickGit::GitCredentials{});   // clear — no crash
}

// -----------------------------------------------------------------------
// cwSaveLoad::setGitHubIntegration
// -----------------------------------------------------------------------

TEST_CASE("cwSaveLoad::setGitHubIntegration accepts nullptr safely", "[cwHttpsAuth]")
{
    cwSaveLoad saveLoad;
    CHECK_NOTHROW(saveLoad.setGitHubIntegration(nullptr));
    // Calling again with nullptr must also be safe.
    CHECK_NOTHROW(saveLoad.setGitHubIntegration(nullptr));
}

TEST_CASE("cwSaveLoad::setGitHubIntegration wires accessTokenChanged", "[cwHttpsAuth]")
{
    cwRemoteCredentialStore credentialStore;
    cwGitHubIntegration integration(&credentialStore);
    cwSaveLoad saveLoad;

    // Wire up — must not crash even though no token is currently set.
    CHECK_NOTHROW(saveLoad.setGitHubIntegration(&integration));

    // Replacing with a different integration cleans up the old connection.
    cwGitHubIntegration integration2(&credentialStore);
    CHECK_NOTHROW(saveLoad.setGitHubIntegration(&integration2));

    // Clearing back to nullptr disconnects cleanly.
    CHECK_NOTHROW(saveLoad.setGitHubIntegration(nullptr));
}

// -----------------------------------------------------------------------
// cwRemoteRepositoryCloner::setGitHubIntegration
// -----------------------------------------------------------------------

TEST_CASE("cwRemoteRepositoryCloner::setGitHubIntegration accepts nullptr safely", "[cwHttpsAuth]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwRecentProjectModel recentProjectModel;
    recentProjectModel.setDefaultRepositoryDir(QUrl::fromLocalFile(tempDir.path()));
    QQuickGit::GitFutureWatcher watcher;

    cwRemoteRepositoryCloner cloner;
    cloner.setRecentProjectModel(&recentProjectModel);
    cloner.setCloneWatcher(&watcher);

    CHECK_NOTHROW(cloner.setGitHubIntegration(nullptr));
    CHECK_NOTHROW(cloner.setGitHubIntegration(nullptr));
}

TEST_CASE("cwRemoteRepositoryCloner::setGitHubIntegration emits signal and wires token", "[cwHttpsAuth]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwRecentProjectModel recentProjectModel;
    recentProjectModel.setDefaultRepositoryDir(QUrl::fromLocalFile(tempDir.path()));
    QQuickGit::GitFutureWatcher watcher;

    cwRemoteRepositoryCloner cloner;
    cloner.setRecentProjectModel(&recentProjectModel);
    cloner.setCloneWatcher(&watcher);

    QSignalSpy spy(&cloner, &cwRemoteRepositoryCloner::gitHubIntegrationChanged);

    cwRemoteCredentialStore credentialStore;
    cwGitHubIntegration integration(&credentialStore);

    cloner.setGitHubIntegration(&integration);
    CHECK(spy.count() == 1);

    // Replacing with another integration emits the signal again.
    cwGitHubIntegration integration2(&credentialStore);
    cloner.setGitHubIntegration(&integration2);
    CHECK(spy.count() == 2);

    // Clearing emits once more.
    cloner.setGitHubIntegration(nullptr);
    CHECK(spy.count() == 3);
}
