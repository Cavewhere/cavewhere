//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRootData.h"
#include "cwJobSettings.h"
#include "cwScrapManager.h"
#include "cwLinePlotManager.h"
#include "LfsBatchClient.h"

//Qt includes
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QCoreApplication>
#include <qtkeychain/keychain.h>

namespace {
constexpr int KeychainWaitMs = 5000;
const QString KeychainService = QStringLiteral("CaveWhere");
const QString KeychainTokenKey = QStringLiteral("GitHubAccessToken");

struct KeychainState
{
    bool success = false;
    bool found = false;
    QString token;
};

static bool waitForJob(QKeychain::Job* job)
{
    QSignalSpy spy(job, &QKeychain::Job::finished);
    job->start();
    if (!spy.wait(KeychainWaitMs)) {
        return false;
    }
    return true;
}

static KeychainState readKeychainToken()
{
    KeychainState state;
    QKeychain::ReadPasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(KeychainTokenKey);

    if (!waitForJob(&job)) {
        return state;
    }

    if (job.error() == QKeychain::NoError) {
        state.success = true;
        state.found = true;
        state.token = job.textData();
        return state;
    }

    if (job.error() == QKeychain::EntryNotFound) {
        state.success = true;
        state.found = false;
        return state;
    }

    return state;
}

static bool writeKeychainToken(const QString& token)
{
    QKeychain::WritePasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(KeychainTokenKey);
    job.setTextData(token);
    if (!waitForJob(&job)) {
        return false;
    }
    return job.error() == QKeychain::NoError;
}

static bool deleteKeychainToken()
{
    QKeychain::DeletePasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(KeychainTokenKey);
    if (!waitForJob(&job)) {
        return false;
    }

    return job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound;
}
}

TEST_CASE("cwRootData should update the managers with auto update correctly", "[cwRootData]") {

    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    cwRootData rootData;
    CHECK(rootData.linePlotManager()->automaticUpdate() == false);
    CHECK(rootData.scrapManager()->automaticUpdate() == false);

    cwJobSettings::instance()->setAutomaticUpdate(true);

    CHECK(rootData.linePlotManager()->automaticUpdate() == true);
    CHECK(rootData.scrapManager()->automaticUpdate() == true);
}

TEST_CASE("cwRootData should install GitHub LFS auth provider", "[cwRootData]")
{
    cwRootData rootData;
    auto provider = QQuickGit::LfsBatchClient::lfsAuthProvider();
    REQUIRE(provider != nullptr);
}

TEST_CASE("cwRootData GitHub LFS auth provider should use GitHub integration token", "[cwRootData]")
{
    const KeychainState previous = readKeychainToken();
    if (!previous.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }

    struct RestoreGuard {
        KeychainState previous;
        ~RestoreGuard() {
            if (!previous.success) {
                return;
            }

            if (previous.found) {
                writeKeychainToken(previous.token);
            } else {
                deleteKeychainToken();
            }
        }
    } restoreGuard{previous};

    const QString expectedToken = QStringLiteral("cw-test-github-lfs-token");
    if (!writeKeychainToken(expectedToken)) {
        SKIP("Skipping: Could not write test token to keychain");
    }

    cwRootData rootData;
    auto* integration = rootData.gitHubIntegration();
    REQUIRE(integration != nullptr);

    integration->setActive(true);

    auto provider = QQuickGit::LfsBatchClient::lfsAuthProvider();
    REQUIRE(provider != nullptr);

    const QByteArray expectedHeader = QByteArrayLiteral("Basic ")
        + QByteArrayLiteral("x-access-token:")
              .append(expectedToken.toUtf8())
              .toBase64();

    QByteArray actualHeader;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < KeychainWaitMs) {
        actualHeader = provider->authorizationHeader(
            QUrl(QStringLiteral("https://github.com/Cavewhere/PhakeCave3000.git/info/lfs")));
        if (actualHeader == expectedHeader) {
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    CHECK(actualHeader == expectedHeader);
    CHECK(provider->authorizationHeader(QUrl(QStringLiteral("https://gitlab.com/group/repo.git/info/lfs"))).isEmpty());
}
