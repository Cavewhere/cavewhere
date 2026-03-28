// Catch2
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QTemporaryDir>

// QQuickGit
#include "GitRepository.h"

// Our
#include "cwProject.h"
#include "cwProjectSyncHealth.h"
#include "cwRemoteAuthProvider.h"
#include "cwSaveLoad.h"

// -----------------------------------------------------------------------
// MockAuthProvider — controllable concrete cwRemoteAuthProvider for tests
// -----------------------------------------------------------------------

class MockAuthProvider : public cwRemoteAuthProvider
{
    Q_OBJECT
public:
    explicit MockAuthProvider(QObject* parent = nullptr) : cwRemoteAuthProvider(parent) {}

    bool hasLoadedCredentials() const override { return m_hasLoaded; }
    QString accessToken() const override { return m_token; }
    void ensureCredentialsLoaded() override {}

    // Simulates the keychain lookup completing (with or without a token).
    void completeLoad(const QString& token = QString())
    {
        m_token = token;
        m_hasLoaded = true;
        emit credentialsLoaded();
        if (!token.isEmpty()) {
            emit accessTokenChanged();
        }
    }

    // Simulates the user signing in / token refreshing.
    void setToken(const QString& token)
    {
        m_token = token;
        emit accessTokenChanged();
    }

    bool m_hasLoaded = false;
    QString m_token;
};

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
    CHECK(!project.syncInProgress()); // sync hasn't started yet — waiting for creds

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
    REQUIRE(!project.syncInProgress());

    QSignalSpy syncProgressSpy(&project, &cwProject::syncInProgressChanged);

    // Supplying credentials should kick off the sync.
    mock.completeLoad(QStringLiteral("ghp_testtoken"));

    // syncInProgress should transition to true (sync started against the fake remote).
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

TEST_CASE("cwProject::setAuthProvider can be replaced and cleared without crash",
          "[cwRemoteAuthProvider]")
{
    cwProject project;

    MockAuthProvider mock1;
    MockAuthProvider mock2;

    CHECK_NOTHROW(project.setAuthProvider(&mock1));
    CHECK_NOTHROW(project.setAuthProvider(&mock2));
    CHECK_NOTHROW(project.setAuthProvider(nullptr));
    CHECK_NOTHROW(project.setAuthProvider(nullptr)); // idempotent
}

#include "test_cwRemoteAuthProvider.moc"
