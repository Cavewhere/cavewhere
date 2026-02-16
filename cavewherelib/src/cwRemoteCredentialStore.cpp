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
        deleteAccessToken(provider, accountId, context);
        return;
    }

    auto* job = new QKeychain::WritePasswordJob(keychainService(), this);
    job->setAutoDelete(true);
    job->setKey(key);
    job->setTextData(token);
    QObject* receiver = context ? context : this;
    QObject::connect(job, &QKeychain::Job::finished, receiver, [job]() {
        if (job->error() != QKeychain::NoError) {
            qWarning() << "Failed to save remote access token:" << job->errorString();
        }
    });
    job->start();
}

void cwRemoteCredentialStore::deleteAccessToken(cwRemoteAccountModel::Provider provider,
                                                const QString& accountId,
                                                QObject* context)
{
    const QString key = accessTokenKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }

    auto* job = new QKeychain::DeletePasswordJob(keychainService(), this);
    job->setAutoDelete(true);
    job->setKey(key);
    QObject* receiver = context ? context : this;
    QObject::connect(job, &QKeychain::Job::finished, receiver, [job]() {
        if (job->error() != QKeychain::NoError && job->error() != QKeychain::EntryNotFound) {
            qWarning() << "Failed to delete remote access token:" << job->errorString();
        }
    });
    job->start();
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

    auto* job = new QKeychain::ReadPasswordJob(keychainService(), const_cast<cwRemoteCredentialStore*>(this));
    job->setAutoDelete(true);
    job->setKey(key);
    QObject* receiver = context ? context : const_cast<cwRemoteCredentialStore*>(this);
    QObject::connect(job, &QKeychain::Job::finished, receiver, [job, callback]() {
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
    const QString normalizedAccountId = accountId.trimmed();
    if (normalizedAccountId.isEmpty()) {
        return {};
    }

    return QStringLiteral("RemoteAccount/%1/%2/AccessToken")
        .arg(cwRemoteAccountModel::providerToString(provider), normalizedAccountId);
}

QString cwRemoteCredentialStore::keychainService()
{
    return KeychainService;
}
