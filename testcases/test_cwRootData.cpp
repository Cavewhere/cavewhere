//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRootData.h"
#include "cwJobSettings.h"
#include "cwScrapManager.h"
#include "cwLinePlotManager.h"
#include "cwRemoteServices.h"
#include "cwGitHubIntegration.h"
#include "cwRemoteCredentialStore.h"
#include "cwRemoteAccountModel.h"
#include "cwRemoteBindingStore.h"
#include "LfsBatchClient.h"

//Qt includes
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QVector>
#include <qtkeychain/keychain.h>

namespace {
constexpr int KeychainWaitMs = 5000;
const QString KeychainService = QStringLiteral("CaveWhere");

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

static KeychainState readKeychainToken(const QString& key)
{
    KeychainState state;
    QKeychain::ReadPasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(key);

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

static bool writeKeychainToken(const QString& key, const QString& token)
{
    QKeychain::WritePasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(key);
    job.setTextData(token);
    if (!waitForJob(&job)) {
        return false;
    }
    return job.error() == QKeychain::NoError;
}

static bool deleteKeychainToken(const QString& key)
{
    QKeychain::DeletePasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(key);
    if (!waitForJob(&job)) {
        return false;
    }

    return job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound;
}

static QByteArray expectedLfsHeader(const QString& token)
{
    return QByteArrayLiteral("Basic ")
           + QByteArrayLiteral("x-access-token:")
                 .append(token.toUtf8())
                 .toBase64();
}

static QByteArray waitForHeader(QQuickGit::LfsAuthProvider* provider,
                                const QUrl& url,
                                const QByteArray& expectedHeader,
                                int timeoutMs = KeychainWaitMs)
{
    QByteArray actual;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        actual = provider->authorizationHeader(url);
        if (actual == expectedHeader) {
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return actual;
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
    cwRootData rootData;
    auto* accountModel = rootData.remote()->accountModel();
    auto* bindingStore = rootData.remote()->bindingStore();
    auto* accountCoordinator = rootData.remote()->accountCoordinator();
    REQUIRE(accountModel != nullptr);
    REQUIRE(bindingStore != nullptr);
    REQUIRE(accountCoordinator != nullptr);

    const QString accountIdA = accountModel->upsertAccount(cwRemoteAccountModel::Provider::GitHub,
                                                            QStringLiteral("cwtestuser-a"));
    const QString accountIdB = accountModel->upsertAccount(cwRemoteAccountModel::Provider::GitHub,
                                                            QStringLiteral("cwtestuser-b"));
    REQUIRE(!accountIdA.isEmpty());
    REQUIRE(!accountIdB.isEmpty());
    REQUIRE(accountIdA != accountIdB);

    accountModel->setAuthState(accountIdA, cwRemoteAccountModel::AuthState::Authorized);
    accountModel->setAuthState(accountIdB, cwRemoteAccountModel::AuthState::Authorized);
    accountModel->setActiveAccount(cwRemoteAccountModel::Provider::GitHub, accountIdB);

    const QString tokenKeyA = cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitHub,
                                                                       accountIdA);
    const QString tokenKeyB = cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitHub,
                                                                       accountIdB);
    REQUIRE(!tokenKeyA.isEmpty());
    REQUIRE(!tokenKeyB.isEmpty());

    const KeychainState previousA = readKeychainToken(tokenKeyA);
    const KeychainState previousB = readKeychainToken(tokenKeyB);
    if (!previousA.success || !previousB.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }

    struct RestoreEntry {
        KeychainState previous;
        QString key;
    };

    struct RestoreGuard {
        QVector<RestoreEntry> entries;
        ~RestoreGuard() {
            for (const RestoreEntry& entry : entries) {
                if (!entry.previous.success) {
                    continue;
                }
                if (entry.previous.found) {
                    writeKeychainToken(entry.key, entry.previous.token);
                } else {
                    deleteKeychainToken(entry.key);
                }
            }
        }
    } restoreGuard{{RestoreEntry{previousA, tokenKeyA},
                    RestoreEntry{previousB, tokenKeyB}}};

    const QString tokenA = QStringLiteral("cw-test-github-lfs-token-a");
    const QString tokenB = QStringLiteral("cw-test-github-lfs-token-b");
    if (!writeKeychainToken(tokenKeyA, tokenA) || !writeKeychainToken(tokenKeyB, tokenB)) {
        SKIP("Skipping: Could not write test token to keychain");
    }

    auto* integration = rootData.remote()->gitHubIntegration();
    REQUIRE(integration != nullptr);
    integration->setActiveAccountId(accountIdB);

    integration->setActive(true);

    auto provider = QQuickGit::LfsBatchClient::lfsAuthProvider();
    REQUIRE(provider != nullptr);

    const QByteArray expectedHeaderA = expectedLfsHeader(tokenA);
    const QByteArray expectedHeaderB = expectedLfsHeader(tokenB);
    const QUrl githubRepoA(QStringLiteral("https://github.com/Cavewhere/PhakeCave3000.git/info/lfs"));
    const QUrl githubRepoB(QStringLiteral("https://github.com/Cavewhere/AnotherRepo.git/info/lfs"));

    // Unbound GitHub URLs should use active-account fallback (account B).
    const QByteArray unboundHeader = waitForHeader(provider.get(), githubRepoA, expectedHeaderB);
    CHECK(unboundHeader == expectedHeaderB);

    // Bound URL should resolve to bound account token (account A), not active fallback.
    bindingStore->bindRemoteToAccount(githubRepoA.toString(), accountIdA);
    const QByteArray boundHeader = waitForHeader(provider.get(), githubRepoA, expectedHeaderA);
    CHECK(boundHeader == expectedHeaderA);

    // If bound account becomes non-authorized, provider should fallback to active account (account B).
    accountModel->setAuthState(accountIdA, cwRemoteAccountModel::AuthState::SignedOut);
    const QByteArray fallbackHeader = waitForHeader(provider.get(), githubRepoA, expectedHeaderB);
    CHECK(fallbackHeader == expectedHeaderB);

    // Another bound URL to active account should still resolve correctly.
    bindingStore->bindRemoteToAccount(githubRepoB.toString(), accountIdB);
    const QByteArray activeBoundHeader = waitForHeader(provider.get(), githubRepoB, expectedHeaderB);
    CHECK(activeBoundHeader == expectedHeaderB);

    // Token invalidation should revoke account B and remove its LFS authorization.
    REQUIRE(QMetaObject::invokeMethod(integration,
                                      "tokenInvalidated",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, accountIdB),
                                      Q_ARG(QString, QStringLiteral("expired"))));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    CHECK(accountModel->accountById(accountIdB).authState
          == static_cast<int>(cwRemoteAccountModel::AuthState::Revoked));
    CHECK(provider->authorizationHeader(githubRepoB).isEmpty());

    CHECK(provider->authorizationHeader(QUrl(QStringLiteral("https://gitlab.com/group/repo.git/info/lfs"))).isEmpty());
}
