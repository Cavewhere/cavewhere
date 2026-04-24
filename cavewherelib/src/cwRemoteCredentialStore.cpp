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

void cwRemoteCredentialStore::writeCredentialBlob(cwRemoteAccountModel::Provider provider,
                                                  const QString& accountId,
                                                  const QByteArray& value,
                                                  QObject* context)
{
    const QString key = credentialBlobKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
    auto* job = new QKeychain::WritePasswordJob(keychainService());
    job->setAutoDelete(true);
    job->setKey(key);
    job->setBinaryData(value);
    QObject* receiver = context ? context : this;
    QObject::connect(job, &QKeychain::Job::finished, receiver, [job]() {
        if (job->error() != QKeychain::NoError) {
            qWarning() << "Failed to save remote credential:" << job->key() << job->errorString();
        }
    });
    job->start();
}

void cwRemoteCredentialStore::deleteCredentialBlob(cwRemoteAccountModel::Provider provider,
                                                   const QString& accountId,
                                                   QObject* context)
{
    const QString key = credentialBlobKey(provider, accountId);
    if (key.isEmpty()) {
        return;
    }
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

void cwRemoteCredentialStore::readCredentialBlob(cwRemoteAccountModel::Provider provider,
                                                 const QString& accountId,
                                                 QObject* context,
                                                 BlobReadCallback callback) const
{
    const QString key = credentialBlobKey(provider, accountId);
    if (key.isEmpty() || !callback) {
        return;
    }
    auto* job = new QKeychain::ReadPasswordJob(keychainService());
    job->setAutoDelete(true);
    job->setKey(key);
    QObject* receiver = context ? context : const_cast<cwRemoteCredentialStore*>(this);
    QObject::connect(job, &QKeychain::Job::finished, receiver,
                     [job, callback = std::move(callback)]() {
        BlobReadResult result;
        if (job->error() == QKeychain::NoError) {
            result.success = true;
            result.found = true;
            result.value = job->binaryData();
        } else if (job->error() == QKeychain::EntryNotFound) {
            result.success = true;
            result.found = false;
        }
        callback(result);
    });
    job->start();
}

QString cwRemoteCredentialStore::credentialBlobKey(cwRemoteAccountModel::Provider provider,
                                                   const QString& accountId)
{
    const QString normalizedAccountId = accountId.trimmed();
    if (normalizedAccountId.isEmpty()) {
        return {};
    }

    return QStringLiteral("RemoteAccount/%1/%2/Credentials")
        .arg(cwRemoteAccountModel::providerToString(provider), normalizedAccountId);
}

QString cwRemoteCredentialStore::keychainService()
{
    return KeychainService;
}
