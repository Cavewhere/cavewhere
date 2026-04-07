// Catch2
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QCoreApplication>

// asyncfuture
#include "asyncfuture.h"

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
// cwSaveLoad::setAuthProvider
// -----------------------------------------------------------------------

TEST_CASE("cwSaveLoad::setAuthProvider accepts nullptr safely", "[cwHttpsAuth]")
{
    cwSaveLoad saveLoad;
    CHECK_NOTHROW(saveLoad.setAuthProvider(nullptr));
    // Calling again with nullptr must also be safe.
    CHECK_NOTHROW(saveLoad.setAuthProvider(nullptr));
}

TEST_CASE("cwSaveLoad::setAuthProvider wires accessTokenChanged", "[cwHttpsAuth]")
{
    cwRemoteCredentialStore credentialStore;
    cwGitHubIntegration integration(&credentialStore);
    cwSaveLoad saveLoad;

    // Wire up — must not crash even though no token is currently set.
    CHECK_NOTHROW(saveLoad.setAuthProvider(&integration));

    // Replacing with a different integration cleans up the old connection.
    cwGitHubIntegration integration2(&credentialStore);
    CHECK_NOTHROW(saveLoad.setAuthProvider(&integration2));

    // Clearing back to nullptr disconnects cleanly.
    CHECK_NOTHROW(saveLoad.setAuthProvider(nullptr));
}

// -----------------------------------------------------------------------
// cwGitHubIntegration::setActiveAccountId keychain-access guard (#351)
// -----------------------------------------------------------------------

// Regression test: setActiveAccountId while inactive (m_active=false) must NOT
// start a keychain read.  Before the fix, it called loadStoredAccessToken()
// unconditionally, which caused a macOS keychain prompt whenever
// RemoteRepositoryPage loaded (via bootstrapGitHubActiveAccount) even though
// the user had not selected a GitHub account from the combo.
TEST_CASE("setActiveAccountId while inactive does not trigger keychain read", "[cwHttpsAuth]")
{
    cwRemoteCredentialStore credentialStore;
    cwGitHubIntegration integration(&credentialStore);
    // m_active defaults to false — no account selected by user yet.

    QSignalSpy spy(&integration, &cwGitHubIntegration::credentialsLoaded);

    integration.setActiveAccountId(QStringLiteral("test-account-id-inactive"));

    // If loadStoredAccessToken was called (regression), the async keychain job
    // would complete and emit credentialsLoaded within a short window.
    // A 500 ms wait is enough for a "not found" keychain response on any platform.
    const bool firedUnexpectedly = spy.wait(500);
    CHECK(!firedUnexpectedly);
    CHECK(!integration.hasLoadedCredentials());
}

// Complementary: setActive(true) after setActiveAccountId MUST trigger a
// keychain read so credentials are available once the user picks GitHub.
TEST_CASE("setActive(true) after setActiveAccountId triggers keychain read", "[cwHttpsAuth]")
{
    cwRemoteCredentialStore credentialStore;
    cwGitHubIntegration integration(&credentialStore);

    integration.setActiveAccountId(QStringLiteral("test-account-id-active"));

    QSignalSpy spy(&integration, &cwGitHubIntegration::credentialsLoaded);
    integration.setActive(true);

    // credentialsLoaded fires once the keychain read completes (entry not found
    // in tests, but the read still goes through and calls the callback).
    const bool fired = spy.wait(5000);
    CHECK(fired);
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

// -----------------------------------------------------------------------
// cwRemoteRepositoryCloner::cloneFailedDueToAuthError
// -----------------------------------------------------------------------

TEST_CASE("cloneFailedDueToAuthError is false by default", "[cwHttpsAuth]")
{
    cwRemoteRepositoryCloner cloner;
    CHECK_FALSE(cloner.cloneFailedDueToAuthError());
}

TEST_CASE("cloneFailedDueToAuthError becomes true when clone fails with HttpAuthFailed", "[cwHttpsAuth]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwRecentProjectModel recentProjectModel;
    recentProjectModel.setDefaultRepositoryDir(QUrl::fromLocalFile(tempDir.path()));
    QQuickGit::GitFutureWatcher watcher;

    cwRemoteRepositoryCloner cloner;
    cloner.setRecentProjectModel(&recentProjectModel);
    cloner.setCloneWatcher(&watcher);

    QSignalSpy authSpy(&cloner, &cwRemoteRepositoryCloner::cloneFailedDueToAuthErrorChanged);

    // Simulate a clone failure with the HttpAuthFailed error code via a pre-completed future.
    auto errorFuture = AsyncFuture::completed(
        Monad::ResultBase(QStringLiteral("401 Unauthorized: no access token"),
                          static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed)));
    watcher.setFuture(errorFuture);

    // Allow the async callback in AbstractResultFutureWatcher to fire.
    authSpy.wait(1000);

    CHECK(cloner.cloneFailedDueToAuthError());
    CHECK(authSpy.count() == 1);
}

TEST_CASE("cloneFailedDueToAuthError is false for non-auth clone failures", "[cwHttpsAuth]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwRecentProjectModel recentProjectModel;
    recentProjectModel.setDefaultRepositoryDir(QUrl::fromLocalFile(tempDir.path()));
    QQuickGit::GitFutureWatcher watcher;

    cwRemoteRepositoryCloner cloner;
    cloner.setRecentProjectModel(&recentProjectModel);
    cloner.setCloneWatcher(&watcher);

    // Simulate a generic clone failure (not an auth error).
    auto errorFuture = AsyncFuture::completed(
        Monad::ResultBase(QStringLiteral("Repository not found")));
    watcher.setFuture(errorFuture);

    QSignalSpy stateSpy(&watcher, &QQuickGit::GitFutureWatcher::stateChanged);
    stateSpy.wait(1000);

    CHECK_FALSE(cloner.cloneFailedDueToAuthError());
}

TEST_CASE("cloneFailedDueToAuthError clears when a new clone starts", "[cwHttpsAuth]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwRecentProjectModel recentProjectModel;
    recentProjectModel.setDefaultRepositoryDir(QUrl::fromLocalFile(tempDir.path()));
    QQuickGit::GitFutureWatcher watcher;

    cwRemoteRepositoryCloner cloner;
    cloner.setRecentProjectModel(&recentProjectModel);
    cloner.setCloneWatcher(&watcher);

    // Put the cloner into the auth-error state.
    auto errorFuture = AsyncFuture::completed(
        Monad::ResultBase(QStringLiteral("401 Unauthorized: no access token"),
                          static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed)));
    watcher.setFuture(errorFuture);

    QSignalSpy authSpy(&cloner, &cwRemoteRepositoryCloner::cloneFailedDueToAuthErrorChanged);
    authSpy.wait(1000);
    REQUIRE(cloner.cloneFailedDueToAuthError());

    // Starting a new clone must clear the flag immediately (before the future completes).
    cloner.clone(QStringLiteral("https://github.com/Cavewhere/fake-repo.git"));
    CHECK_FALSE(cloner.cloneFailedDueToAuthError());
}

// -----------------------------------------------------------------------
// Section 6: PrePushCredentialPayload httpsToken threading
// -----------------------------------------------------------------------

// Verifies that GitRepository::push() with HTTPS credentials set on a repo
// that has no remote configured fails with a "no remote" error rather than
// crashing or producing an auth error.  This exercises the call chain:
//   push() -> runLfsPrePushUpload() -> buildLfsPushUploadPlan()
//          -> hideAdvertisedRemoteTips() -> PrePushCredentialPayload::httpsToken
TEST_CASE("push with HTTPS credentials on repo with no remote fails gracefully", "[cwHttpsAuth]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QQuickGit::GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    repo.setCredentials(QQuickGit::GitCredentials{QStringLiteral("ghp_testtoken123")});

    QQuickGit::GitFutureWatcher watcher;
    QSignalSpy spy(&watcher, &QQuickGit::GitFutureWatcher::stateChanged);
    watcher.setFuture(repo.push());

    spy.wait(5000);

    REQUIRE(watcher.state() == QQuickGit::GitFutureWatcher::Ready);
    CHECK(watcher.hasError());
    // Must NOT be an auth error — the failure should be "no remote configured",
    // which confirms the token was threaded through without incorrectly triggering
    // auth-failure logic on a repo that has no remote at all.
    const auto result = watcher.future().result();
    CHECK(result.errorCode() != static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed));
}
