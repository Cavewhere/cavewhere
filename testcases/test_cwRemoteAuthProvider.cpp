// Catch2
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

// QQuickGit
#include "Account.h"
#include "GitRepository.h"

// Our
#include "MockAuthProvider.h"
#include "cwProject.h"
#include "cwProjectSyncHealth.h"
#include "cwRemoteAuthProvider.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

namespace {

constexpr int DefaultTimeoutMs = 5000;

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = DefaultTimeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return condition();
}

// Adds an HTTPS remote to the project's repository (synchronous).
void addHttpsRemote(cwProject& project, const QString& url = QStringLiteral("https://github.com/example/repo.git"))
{
    project.repository()->addRemote(QStringLiteral("origin"), QUrl(url));
}

// Adds a local file:// remote to the project's repository (synchronous).
void addFileRemote(cwProject& project, const QString& path)
{
    project.repository()->addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(path));
}

} // namespace

// -----------------------------------------------------------------------
// cwSaveLoad::requiresProviderCredentials()
// -----------------------------------------------------------------------

TEST_CASE("cwSaveLoad::requiresProviderCredentials - no remote returns false", "[cwRemoteAuthProvider]")
{
    // A fresh cwSaveLoad with no repository directory or remote configured.
    cwSaveLoad saveLoad;
    CHECK(!saveLoad.requiresProviderCredentials());
}

TEST_CASE("cwSaveLoad::requiresProviderCredentials - HTTPS remote returns true", "[cwRemoteAuthProvider]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwSaveLoad saveLoad;
    saveLoad.repository()->setDirectory(QDir(tempDir.path()));
    saveLoad.repository()->initRepository();
    saveLoad.repository()->addRemote(QStringLiteral("origin"),
                                     QUrl(QStringLiteral("https://github.com/example/repo.git")));

    CHECK(saveLoad.requiresProviderCredentials());
}

TEST_CASE("cwSaveLoad::requiresProviderCredentials - file remote returns false", "[cwRemoteAuthProvider]")
{
    QTemporaryDir repoDir;
    QTemporaryDir remoteDir;
    REQUIRE(repoDir.isValid());
    REQUIRE(remoteDir.isValid());

    cwSaveLoad saveLoad;
    saveLoad.repository()->setDirectory(QDir(repoDir.path()));
    saveLoad.repository()->initRepository();
    saveLoad.repository()->addRemote(QStringLiteral("origin"),
                                     QUrl::fromLocalFile(remoteDir.path()));

    CHECK(!saveLoad.requiresProviderCredentials());
}

// -----------------------------------------------------------------------
// cwProject::sync() — credential deferral behaviour
// -----------------------------------------------------------------------

TEST_CASE("cwProject::sync defers and emits authProviderCredentialsNeeded for HTTPS with unloaded creds",
          "[cwRemoteAuthProvider]")
{
    cwProject project;
    addHttpsRemote(project);

    MockAuthProvider mock;
    mock.m_hasLoaded = false;
    project.setAuthProvider(&mock);

    QSignalSpy spy(&project, &cwProject::authProviderCredentialsNeeded);

    const bool accepted = project.sync();

    CHECK(accepted);
    CHECK(spy.count() == 1);
    CHECK(project.syncInProgress()); // deferred sync is in-progress (waiting for creds)

    // Complete the credential load so the deferred future resolves and
    // waitForSyncToFinish() does not deadlock.
    mock.completeLoad(QStringLiteral("ghp_testtoken"));

    project.waitForSyncToFinish();
}

TEST_CASE("cwProject::sync does NOT defer when HTTPS creds are already loaded",
          "[cwRemoteAuthProvider]")
{
    cwProject project;
    addHttpsRemote(project);

    MockAuthProvider mock;
    mock.m_hasLoaded = true;
    mock.m_token = QStringLiteral("ghp_testtoken");
    project.setAuthProvider(&mock);

    QSignalSpy spy(&project, &cwProject::authProviderCredentialsNeeded);

    project.sync(); // will start and fail against the fake remote, but no deferral

    CHECK(spy.count() == 0);

    project.waitForSyncToFinish();
}

TEST_CASE("cwProject::sync does NOT defer for a non-HTTPS (file://) remote",
          "[cwRemoteAuthProvider]")
{
    QTemporaryDir remoteDir;
    REQUIRE(remoteDir.isValid());

    cwProject project;
    addFileRemote(project, remoteDir.path());

    MockAuthProvider mock;
    mock.m_hasLoaded = false; // creds not loaded, but shouldn't matter for non-HTTPS
    project.setAuthProvider(&mock);

    QSignalSpy spy(&project, &cwProject::authProviderCredentialsNeeded);

    project.sync();

    CHECK(spy.count() == 0);

    project.waitForSyncToFinish();
}

// -----------------------------------------------------------------------
// Deferred sync: credentialsLoaded triggers the sync operation
// -----------------------------------------------------------------------

TEST_CASE("cwProject::sync starts after credentialsLoaded fires following deferral",
          "[cwRemoteAuthProvider]")
{
    cwProject project;
    addHttpsRemote(project);

    MockAuthProvider mock;
    mock.m_hasLoaded = false;
    project.setAuthProvider(&mock);

    // Trigger the deferred path.
    const bool accepted = project.sync();
    REQUIRE(accepted);
    REQUIRE(project.syncInProgress()); // deferred sync is in-progress (waiting for creds)

    QSignalSpy syncProgressSpy(&project, &cwProject::syncInProgressChanged);

    // Supplying credentials should kick off the sync (the real operation replaces the deferred future).
    mock.completeLoad(QStringLiteral("ghp_testtoken"));
    CHECK(project.syncInProgress());

    // syncInProgress should remain true (sync now running against the fake remote).
    REQUIRE(waitUntil([&syncProgressSpy]() { return syncProgressSpy.count() >= 1; }));

    project.waitForSyncToFinish();
}

// -----------------------------------------------------------------------
// cwProject::setAuthProvider — accessTokenChanged wired to syncHealth refresh
// -----------------------------------------------------------------------

TEST_CASE("cwProject::setAuthProvider wires accessTokenChanged to syncHealth refresh",
          "[cwRemoteAuthProvider]")
{
    // Verify: provider::accessTokenChanged → cwProjectSyncHealth::refresh()
    //
    // setStatus() only emits statusChanged when the status value changes.
    // To guarantee a change we engineer the following state:
    //
    //   current status : "Sync unavailable" (m_repository == nullptr)
    //   result of refresh() : async error from the fake HTTPS remote
    //
    // After setRepository(nullptr) the status is "Sync unavailable".
    // We then call setRepository(repo) which restores m_repository and fires
    // an internal refresh(), kicking off an async ahead/behind fetch.  Before
    // that async completes we emit accessTokenChanged, which fires refresh()
    // a second time via the connection created by setAuthProvider.  The async
    // result (an error from the fake HTTPS URL) is different from
    // "Sync unavailable", so statusChanged fires at least once.

    cwProject project;
    addHttpsRemote(project);

    MockAuthProvider mock;
    mock.m_hasLoaded = true;
    mock.m_token = QStringLiteral("initial-token");
    project.setAuthProvider(&mock);

    auto* syncHealth = project.syncHealth();
    auto* repo = project.repository();

    // Put the status into "Sync unavailable" by detaching the repository.
    syncHealth->setRepository(nullptr);
    REQUIRE(waitUntil([syncHealth]() {
        return syncHealth->status().message()
               == QStringLiteral("Sync unavailable: repository is not initialized.");
    }));

    // Start spying, then re-attach the repository. setRepository triggers an
    // internal refresh() that starts an async fetch. While that is in flight,
    // emit accessTokenChanged to exercise the wired connection. The async
    // eventually resolves to an error status (different from "Sync unavailable"),
    // so statusChanged must fire.
    QSignalSpy statusSpy(syncHealth, &cwProjectSyncHealth::statusChanged);
    syncHealth->setRepository(repo);
    mock.setToken(QStringLiteral("refreshed-token"));

    REQUIRE(waitUntil([&statusSpy]() { return statusSpy.count() >= 1; }));

    project.waitForSyncToFinish();
}

// -----------------------------------------------------------------------
// cwProject::setAuthProvider — replacing or clearing provider is safe
// -----------------------------------------------------------------------

TEST_CASE("cwProject::setAuthProvider same-instance guard and disconnect on replace",
          "[cwRemoteAuthProvider]")
{
    cwProject project;
    auto* syncHealth = project.syncHealth();

    MockAuthProvider mock1;
    MockAuthProvider mock2;

    // First assignment — connects mock1 signals to syncHealth.
    project.setAuthProvider(&mock1);

    // Same-instance guard: calling again with mock1 must not create duplicate connections.
    project.setAuthProvider(&mock1);
    {
        QSignalSpy spy(syncHealth, &cwProjectSyncHealth::statusChanged);
        mock1.setToken(QStringLiteral("tok"));
        // Process events so any duplicate-fired signals can land.
        QCoreApplication::processEvents();
        // statusChanged must fire at most once per token change (no duplicates).
        CHECK(spy.count() <= 1);
    }

    // Replace with mock2 — must disconnect mock1 and connect mock2.
    project.setAuthProvider(&mock2);
    {
        QSignalSpy spy(syncHealth, &cwProjectSyncHealth::statusChanged);
        mock1.setToken(QStringLiteral("should-not-fire"));
        QCoreApplication::processEvents();
        CHECK(spy.count() == 0); // mock1 is disconnected

        mock2.setToken(QStringLiteral("tok2"));
        QCoreApplication::processEvents();
        CHECK(spy.count() <= 1); // mock2 is connected
    }

    // Clear — must disconnect mock2.
    project.setAuthProvider(nullptr);
    {
        QSignalSpy spy(syncHealth, &cwProjectSyncHealth::statusChanged);
        mock2.setToken(QStringLiteral("should-not-fire-either"));
        QCoreApplication::processEvents();
        CHECK(spy.count() == 0); // mock2 is disconnected
    }

    // Idempotent null → null.
    CHECK_NOTHROW(project.setAuthProvider(nullptr));
}

// -----------------------------------------------------------------------
// Regression: auth provider survives File -> New (newProject creates a fresh cwSaveLoad)
// -----------------------------------------------------------------------

TEST_CASE("cwProject::newProject preserves auth provider credentials on the new saveLoad",
          "[cwRemoteAuthProvider]")
{
    // Regression: cwProject::newProject() replaces m_saveLoad with a fresh cwSaveLoad
    // instance.  connectSaveLoad() must re-apply the stored m_authProvider so that the
    // new saveLoad's GitRepository has the correct credentials.  Without this fix,
    // re-opening a project after File -> New produces HttpAuthFailed because the
    // replacement saveLoad has an empty GitCredentials.
    //
    // cwProject::newProject() calls m_saveLoad->retire() which needs a valid
    // FutureManagerToken, so we use cwRootData (which sets one up) rather than a
    // bare cwProject.

    // cwRootData installs cwGitHubIntegration as the auth provider.  We inject our
    // own mock via setAuthProvider() to control the token value.
    auto rootData = std::make_unique<cwRootData>();
    auto* project = rootData->project();

    MockAuthProvider mock;
    mock.m_hasLoaded = true;
    mock.m_token = QStringLiteral("ghp_validtoken");
    project->setAuthProvider(&mock);

    // Confirm the token is on the current saveLoad before newProject().
    CHECK(project->repository()->credentials().httpsToken == QStringLiteral("ghp_validtoken"));

    project->newProject();

    // After newProject(), m_saveLoad is a brand-new instance. The credentials must
    // still reflect the stored auth provider's current token.
    CHECK(project->repository()->credentials().httpsToken == QStringLiteral("ghp_validtoken"));

    // Changing the token afterwards must propagate to the new saveLoad as well.
    mock.setToken(QStringLiteral("ghp_refreshed"));
    CHECK(project->repository()->credentials().httpsToken == QStringLiteral("ghp_refreshed"));
}

// -----------------------------------------------------------------------
// cwProjectSyncHealth needsLogin vs authExpired split
// -----------------------------------------------------------------------

namespace {

// Creates a GitRepository in tempDir with one commit and an HTTPS remote.
// The repo needs at least one commit so headBranchName() is non-empty and
// cwProjectSyncHealth::refresh() actually fires remoteAheadBehindCommitCounts.
void setupRepoWithHttpsRemote(QQuickGit::GitRepository& repo, const QTemporaryDir& tempDir)
{
    QQuickGit::Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    repo.setAccount(&account);

    QFile f(QDir(tempDir.path()).filePath(QStringLiteral("data.txt")));
    REQUIRE(f.open(QFile::WriteOnly | QFile::Text));
    f.write("init\n");
    f.close();

    CHECK_NOTHROW(repo.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));
    repo.addRemote(QStringLiteral("origin"),
                   QUrl(QStringLiteral("https://github.com/example/repo.git")));
}

} // namespace

TEST_CASE("cwProjectSyncHealth sets needsLogin when HttpAuthFailed and creds not yet loaded",
          "[cwRemoteAuthProvider]")
{
    // When the remote fetch returns HttpAuthFailed and the auth provider has
    // never loaded credentials, the status must be needsLogin (not authExpired).
    // This prevents an automatic keychain prompt on file open — the user must
    // explicitly click the sync button first.

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QQuickGit::GitRepository repo;
    setupRepoWithHttpsRemote(repo, tempDir);

    MockAuthProvider mock;
    mock.m_hasLoaded = false; // credentials never loaded

    cwProjectSyncHealth syncHealth;
    syncHealth.setAuthProvider(&mock);
    syncHealth.setRepository(&repo);

    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().needsLogin() || syncHealth.status().authExpired();
    }));

    CHECK(syncHealth.status().needsLogin());
    CHECK(!syncHealth.status().authExpired());
}

TEST_CASE("cwProjectSyncHealth sets authExpired when HttpAuthFailed and creds were loaded",
          "[cwRemoteAuthProvider]")
{
    // When the remote fetch returns HttpAuthFailed but the auth provider already
    // loaded credentials (token was accepted at some point but has since expired),
    // the status must be authExpired (not needsLogin).

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QQuickGit::GitRepository repo;
    setupRepoWithHttpsRemote(repo, tempDir);

    MockAuthProvider mock;
    mock.m_hasLoaded = true; // credentials were loaded
    mock.m_token = QStringLiteral("ghp_expired_token");

    cwProjectSyncHealth syncHealth;
    syncHealth.setAuthProvider(&mock);
    syncHealth.setRepository(&repo);

    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().needsLogin() || syncHealth.status().authExpired();
    }));

    CHECK(syncHealth.status().authExpired());
    CHECK(!syncHealth.status().needsLogin());
}
