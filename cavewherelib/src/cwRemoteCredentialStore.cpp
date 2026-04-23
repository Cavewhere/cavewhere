#include "cwRemoteCredentialStore.h"

#include <QDebug>
#include <qtkeychain/keychain.h>

namespace {
const QString KeychainService = QStringLiteral("CaveWhere");
}

cwRemoteCredentialStore::cwRemoteCredentialStore(QObject* parent) :
    QObject(parent)
{
}

void cwRemoteCredentialStore::writeAccessToken(cwRemoteAccountModel::Provider provider,
                                               const QString& accountId,
                                               const QString& token,
                                               QObject* context)
{
    const QString key = accessTokenKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
    if (token.isEmpty()) {
        deleteValue(key, context);
        return;
    }
    writeValue(key, token, context);
}

void cwRemoteCredentialStore::deleteAccessToken(cwRemoteAccountModel::Provider provider,
                                                const QString& accountId,
                                                QObject* context)
{
    const QString key = accessTokenKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
    deleteValue(key, context);
}

void cwRemoteCredentialStore::readAccessToken(cwRemoteAccountModel::Provider provider,
                                              const QString& accountId,
                                              QObject* context,
                                              ReadCallback callback) const
{
    const QString key = accessTokenKey(provider, accountId);
    if (key.isEmpty() || !callback) {
        return;
    }
    readValue(key, context, std::move(callback));
}

void cwRemoteCredentialStore::writeRefreshToken(cwRemoteAccountModel::Provider provider,
                                                const QString& accountId,
                                                const QString& token,
                                                QObject* context)
{
    const QString key = refreshTokenKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
    if (token.isEmpty()) {
        deleteValue(key, context);
        return;
    }
    writeValue(key, token, context);
}

void cwRemoteCredentialStore::deleteRefreshToken(cwRemoteAccountModel::Provider provider,
                                                 const QString& accountId,
                                                 QObject* context)
{
    const QString key = refreshTokenKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
    deleteValue(key, context);
}

void cwRemoteCredentialStore::readRefreshToken(cwRemoteAccountModel::Provider provider,
                                               const QString& accountId,
                                               QObject* context,
                                               ReadCallback callback) const
{
    const QString key = refreshTokenKey(provider, accountId);
    if (key.isEmpty() || !callback) {
        return;
    }
    readValue(key, context, std::move(callback));
}

void cwRemoteCredentialStore::writeAccessTokenExpiresAt(cwRemoteAccountModel::Provider provider,
                                                        const QString& accountId,
                                                        qint64 epochSeconds,
                                                        QObject* context)
{
    const QString key = accessTokenExpiresAtKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
    if (epochSeconds < 0) {
        deleteValue(key, context);
        return;
    }
    writeValue(key, QString::number(epochSeconds), context);
}

void cwRemoteCredentialStore::deleteAccessTokenExpiresAt(cwRemoteAccountModel::Provider provider,
                                                         const QString& accountId,
                                                         QObject* context)
{
    const QString key = accessTokenExpiresAtKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
    deleteValue(key, context);
}

void cwRemoteCredentialStore::readAccessTokenExpiresAt(cwRemoteAccountModel::Provider provider,
                                                       const QString& accountId,
                                                       QObject* context,
                                                       std::function<void(bool, bool, qint64)> callback) const
{
    const QString key = accessTokenExpiresAtKey(provider, accountId);
    if (key.isEmpty() || !callback) {
        return;
    }
    readValue(key, context, [callback = std::move(callback)](const ReadResult& result) {
        if (!result.success) {
            callback(false, false, -1);
            return;
        }
        if (!result.found) {
            callback(true, false, -1);
            return;
        }
        bool ok = false;
        const qint64 parsed = result.value.toLongLong(&ok);
        callback(true, true, ok ? parsed : -1);
    });
}

void cwRemoteCredentialStore::writeValue(const QString& key, const QString& value, QObject* context)
{
    auto* job = new QKeychain::WritePasswordJob(keychainService());
    job->setAutoDelete(true);
    job->setKey(key);
    job->setTextData(value);
    QObject* receiver = context ? context : this;
    QObject::connect(job, &QKeychain::Job::finished, receiver, [job]() {
        if (job->error() != QKeychain::NoError) {
            qWarning() << "Failed to save remote credential:" << job->key() << job->errorString();
        }
    });
    job->start();
}

void cwRemoteCredentialStore::deleteValue(const QString& key, QObject* context)
{
    auto* job = new QKeychain::DeletePasswordJob(keychainService());
    job->setAutoDelete(true);
    job->setKey(key);
    QObject* receiver = context ? context : this;
    QObject::connect(job, &QKeychain::Job::finished, receiver, [job]() {
        if (job->error() != QKeychain::NoError && job->error() != QKeychain::EntryNotFound) {
            qWarning() << "Failed to delete remote credential:" << job->key() << job->errorString();
        }
    });
    job->start();
}

void cwRemoteCredentialStore::readValue(const QString& key, QObject* context, ReadCallback callback) const
{
    auto* job = new QKeychain::ReadPasswordJob(keychainService());
    job->setAutoDelete(true);
    job->setKey(key);
    QObject* receiver = context ? context : const_cast<cwRemoteCredentialStore*>(this);
    QObject::connect(job, &QKeychain::Job::finished, receiver, [job, callback = std::move(callback)]() {
        ReadResult result;
        if (job->error() == QKeychain::NoError) {
            result.success = true;
            result.found = true;
            result.value = job->textData();
        } else if (job->error() == QKeychain::EntryNotFound) {
            result.success = true;
            result.found = false;
        }
        callback(result);
    });
    job->start();
}

QString cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider provider, const QString& accountId)
{
    return credentialKey(provider, accountId, QLatin1StringView("AccessToken"));
}

QString cwRemoteCredentialStore::refreshTokenKey(cwRemoteAccountModel::Provider provider, const QString& accountId)
{
    return credentialKey(provider, accountId, QLatin1StringView("RefreshToken"));
}

QString cwRemoteCredentialStore::accessTokenExpiresAtKey(cwRemoteAccountModel::Provider provider, const QString& accountId)
{
    return credentialKey(provider, accountId, QLatin1StringView("AccessTokenExpiresAt"));
}

QString cwRemoteCredentialStore::credentialKey(cwRemoteAccountModel::Provider provider,
                                               const QString& accountId,
                                               QLatin1StringView field)
{
    const QString normalizedAccountId = accountId.trimmed();
    if (normalizedAccountId.isEmpty()) {
        return {};
    }

    return QStringLiteral("RemoteAccount/%1/%2/%3")
        .arg(cwRemoteAccountModel::providerToString(provider), normalizedAccountId, QString(field));
}

QString cwRemoteCredentialStore::keychainService()
{
    return KeychainService;
}
