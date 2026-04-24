#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
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
    QByteArray value;
};

bool waitForJob(QKeychain::Job* job)
{
    QSignalSpy spy(job, &QKeychain::Job::finished);
    job->start();
    return spy.wait(KeychainWaitMs);
}

KeychainState readKeychainRaw(const QString& key)
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
        state.value = job.binaryData();
        return state;
    }

    if (job.error() == QKeychain::EntryNotFound) {
        state.success = true;
        state.found = false;
        return state;
    }

    return state;
}

bool writeKeychainRaw(const QString& key, const QByteArray& value)
{
    QKeychain::WritePasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(key);
    job.setBinaryData(value);
    if (!waitForJob(&job)) {
        return false;
    }
    return job.error() == QKeychain::NoError;
}

bool deleteKeychainRaw(const QString& key)
{
    QKeychain::DeletePasswordJob job(KeychainService);
    job.setAutoDelete(false);
    job.setKey(key);
    if (!waitForJob(&job)) {
        return false;
    }
    return job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound;
}

QString pidScopedAccountId(const char* label)
{
    return QStringLiteral("%1-%2").arg(QLatin1StringView(label))
        .arg(QCoreApplication::applicationPid());
}

bool waitFor(const std::function<bool()>& predicate, int timeoutMs = KeychainWaitMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return predicate();
}

struct KeychainRestoreGuard
{
    KeychainState previous;
    QString key;
    ~KeychainRestoreGuard() {
        if (!previous.success) {
            return;
        }
        if (previous.found) {
            writeKeychainRaw(key, previous.value);
        } else {
            deleteKeychainRaw(key);
        }
    }
};
}

TEST_CASE("cwRemoteCredentialStore::credentialBlobKey formatting", "[cwRemoteCredentialStore]")
{
    CHECK(cwRemoteCredentialStore::credentialBlobKey(cwRemoteAccountModel::Provider::GitHub,
                                                      QStringLiteral("abc-123"))
          == QStringLiteral("RemoteAccount/github/abc-123/Credentials"));
    CHECK(cwRemoteCredentialStore::credentialBlobKey(cwRemoteAccountModel::Provider::GitLab,
                                                      QStringLiteral(" id-with-space "))
          == QStringLiteral("RemoteAccount/gitlab/id-with-space/Credentials"));
    CHECK(cwRemoteCredentialStore::credentialBlobKey(cwRemoteAccountModel::Provider::GitHub,
                                                      QString()).isEmpty());
}

TEST_CASE("cwRemoteCredentialStore blob write/read/delete round-trip", "[cwRemoteCredentialStore]")
{
    cwRemoteCredentialStore store;
    const auto provider = cwRemoteAccountModel::Provider::GitHub;
    const QString accountId = pidScopedAccountId("cw-credstore-roundtrip");
    const QString key = cwRemoteCredentialStore::credentialBlobKey(provider, accountId);
    REQUIRE(!key.isEmpty());

    const KeychainState probe = readKeychainRaw(key);
    if (!probe.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }
    KeychainRestoreGuard guard{probe, key};

    QJsonObject expectedObject;
    expectedObject.insert(QStringLiteral("v"), 1);
    expectedObject.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_credstore_test"));
    expectedObject.insert(QStringLiteral("refreshToken"), QStringLiteral("ghr_credstore_test"));
    expectedObject.insert(QStringLiteral("accessTokenExpiresAt"), 1800000000.0);
    const QByteArray expected = QJsonDocument(expectedObject).toJson(QJsonDocument::Compact);

    store.writeCredentialBlob(provider, accountId, expected, &store);

    REQUIRE(waitFor([&]() {
        const KeychainState state = readKeychainRaw(key);
        return state.success && state.found && state.value == expected;
    }));

    bool callbackReceived = false;
    cwRemoteCredentialStore::BlobReadResult readResult;
    store.readCredentialBlob(provider, accountId, &store,
        [&](const cwRemoteCredentialStore::BlobReadResult& result) {
            callbackReceived = true;
            readResult = result;
        });

    REQUIRE(waitFor([&]() { return callbackReceived; }));
    CHECK(readResult.success);
    CHECK(readResult.found);
    CHECK(readResult.value == expected);

    store.deleteCredentialBlob(provider, accountId, &store);
    REQUIRE(waitFor([&]() {
        const KeychainState state = readKeychainRaw(key);
        return state.success && !state.found;
    }));
}

TEST_CASE("cwRemoteCredentialStore read of absent key reports not-found", "[cwRemoteCredentialStore]")
{
    cwRemoteCredentialStore store;
    const auto provider = cwRemoteAccountModel::Provider::GitHub;
    const QString accountId = pidScopedAccountId("cw-credstore-absent");
    const QString key = cwRemoteCredentialStore::credentialBlobKey(provider, accountId);

    const KeychainState probe = readKeychainRaw(key);
    if (!probe.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }
    REQUIRE(deleteKeychainRaw(key));

    bool callbackReceived = false;
    cwRemoteCredentialStore::BlobReadResult readResult;
    store.readCredentialBlob(provider, accountId, &store,
        [&](const cwRemoteCredentialStore::BlobReadResult& result) {
            callbackReceived = true;
            readResult = result;
        });

    REQUIRE(waitFor([&]() { return callbackReceived; }));
    CHECK(readResult.success);
    CHECK_FALSE(readResult.found);
    CHECK(readResult.value.isEmpty());
}

TEST_CASE("cwRemoteCredentialStore concurrent reads observe identical content",
          "[cwRemoteCredentialStore]")
{
    cwRemoteCredentialStore store;
    const auto provider = cwRemoteAccountModel::Provider::GitHub;
    const QString accountId = pidScopedAccountId("cw-credstore-concurrent");
    const QString key = cwRemoteCredentialStore::credentialBlobKey(provider, accountId);
    REQUIRE(!key.isEmpty());

    const KeychainState probe = readKeychainRaw(key);
    if (!probe.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }
    KeychainRestoreGuard guard{probe, key};

    QJsonObject blobObject;
    blobObject.insert(QStringLiteral("v"), 1);
    blobObject.insert(QStringLiteral("accessToken"), QStringLiteral("ghs_concurrent"));
    blobObject.insert(QStringLiteral("refreshToken"), QStringLiteral("ghr_concurrent"));
    const QByteArray blob = QJsonDocument(blobObject).toJson(QJsonDocument::Compact);
    REQUIRE(writeKeychainRaw(key, blob));

    cwRemoteCredentialStore::BlobReadResult first;
    cwRemoteCredentialStore::BlobReadResult second;
    bool firstReceived = false;
    bool secondReceived = false;

    store.readCredentialBlob(provider, accountId, &store,
        [&](const cwRemoteCredentialStore::BlobReadResult& result) {
            firstReceived = true;
            first = result;
        });
    store.readCredentialBlob(provider, accountId, &store,
        [&](const cwRemoteCredentialStore::BlobReadResult& result) {
            secondReceived = true;
            second = result;
        });

    REQUIRE(waitFor([&]() { return firstReceived && secondReceived; }));
    CHECK(first.success);
    CHECK(first.found);
    CHECK(first.value == blob);
    CHECK(second.success);
    CHECK(second.found);
    CHECK(second.value == blob);
}

TEST_CASE("cwRemoteCredentialStore writing an empty blob keeps the item present",
          "[cwRemoteCredentialStore]")
{
    cwRemoteCredentialStore store;
    const auto provider = cwRemoteAccountModel::Provider::GitHub;
    const QString accountId = pidScopedAccountId("cw-credstore-empty");
    const QString key = cwRemoteCredentialStore::credentialBlobKey(provider, accountId);
    REQUIRE(!key.isEmpty());

    const KeychainState probe = readKeychainRaw(key);
    if (!probe.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }
    KeychainRestoreGuard guard{probe, key};

    store.writeCredentialBlob(provider, accountId, QByteArray(), &store);

    REQUIRE(waitFor([&]() {
        const KeychainState state = readKeychainRaw(key);
        return state.success && state.found && state.value.isEmpty();
    }));

    bool callbackReceived = false;
    cwRemoteCredentialStore::BlobReadResult readResult;
    store.readCredentialBlob(provider, accountId, &store,
        [&](const cwRemoteCredentialStore::BlobReadResult& result) {
            callbackReceived = true;
            readResult = result;
        });
    REQUIRE(waitFor([&]() { return callbackReceived; }));
    CHECK(readResult.success);
    CHECK(readResult.found);
    CHECK(readResult.value.isEmpty());
}

TEST_CASE("cwRemoteCredentialStore read of malformed bytes returns them verbatim",
          "[cwRemoteCredentialStore]")
{
    // Store is opaque: it does not parse the blob. Malformed content surfaces to
    // the caller unchanged; interpretation (e.g. cwGitHubCredentials::fromKeychainBytes)
    // is the provider layer's problem.
    cwRemoteCredentialStore store;
    const auto provider = cwRemoteAccountModel::Provider::GitHub;
    const QString accountId = pidScopedAccountId("cw-credstore-garbage");
    const QString key = cwRemoteCredentialStore::credentialBlobKey(provider, accountId);
    REQUIRE(!key.isEmpty());

    const KeychainState probe = readKeychainRaw(key);
    if (!probe.success) {
        SKIP("Skipping: Keychain unavailable in this environment");
    }
    KeychainRestoreGuard guard{probe, key};

    const QByteArray garbage = QByteArrayLiteral("this is not json {");
    REQUIRE(writeKeychainRaw(key, garbage));

    bool callbackReceived = false;
    cwRemoteCredentialStore::BlobReadResult readResult;
    store.readCredentialBlob(provider, accountId, &store,
        [&](const cwRemoteCredentialStore::BlobReadResult& result) {
            callbackReceived = true;
            readResult = result;
        });
    REQUIRE(waitFor([&]() { return callbackReceived; }));
    CHECK(readResult.success);
    CHECK(readResult.found);
    CHECK(readResult.value == garbage);
}
