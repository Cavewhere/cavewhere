#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <qtkeychain/keychain.h>

#include "cwRemoteCredentialStore.h"

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
}

TEST_CASE("cwRemoteCredentialStore access token key formatting", "[cwRemoteCredentialStore]")
{
    CHECK(cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitHub, QStringLiteral("abc-123"))
          == QStringLiteral("RemoteAccount/github/abc-123/AccessToken"));
    CHECK(cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitLab, QStringLiteral(" id-with-space "))
          == QStringLiteral("RemoteAccount/gitlab/id-with-space/AccessToken"));
    CHECK(cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitHub, QString()).isEmpty());
}

TEST_CASE("cwRemoteCredentialStore write/read/delete lifecycle", "[cwRemoteCredentialStore]")
{
    cwRemoteCredentialStore store;
    const QString accountId = QStringLiteral("cw-credential-test-account");
    const QString tokenKey = cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitHub, accountId);
    REQUIRE(!tokenKey.isEmpty());

    const KeychainState previous = readKeychainToken(tokenKey);
    if (!previous.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }

    struct RestoreGuard {
        KeychainState previous;
        QString key;
        ~RestoreGuard() {
            if (!previous.success) {
                return;
            }

            if (previous.found) {
                writeKeychainToken(key, previous.token);
            } else {
                deleteKeychainToken(key);
            }
        }
    } restoreGuard{previous, tokenKey};

    const QString expectedToken = QStringLiteral("cw-credential-store-test-token");
    store.writeAccessToken(cwRemoteAccountModel::Provider::GitHub, accountId, expectedToken, &store);

    QElapsedTimer timer;
    timer.start();
    KeychainState state;
    while (timer.elapsed() < KeychainWaitMs) {
        state = readKeychainToken(tokenKey);
        if (state.success && state.found && state.token == expectedToken) {
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    REQUIRE(state.success);
    CHECK(state.found);
    CHECK(state.token == expectedToken);

    bool callbackReceived = false;
    cwRemoteCredentialStore::ReadResult readResult;
    store.readAccessToken(cwRemoteAccountModel::Provider::GitHub,
                          accountId,
                          &store,
                          [&](const cwRemoteCredentialStore::ReadResult& result) {
                              callbackReceived = true;
                              readResult = result;
                          });

    timer.restart();
    while (timer.elapsed() < KeychainWaitMs && !callbackReceived) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    REQUIRE(callbackReceived);
    CHECK(readResult.success);
    CHECK(readResult.found);
    CHECK(readResult.value == expectedToken);

    store.deleteAccessToken(cwRemoteAccountModel::Provider::GitHub, accountId, &store);
    timer.restart();
    while (timer.elapsed() < KeychainWaitMs) {
        state = readKeychainToken(tokenKey);
        if (state.success && !state.found) {
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    REQUIRE(state.success);
    CHECK_FALSE(state.found);
}
