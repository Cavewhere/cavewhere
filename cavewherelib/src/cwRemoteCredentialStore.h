#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <functional>

#include "CaveWhereLibExport.h"
#include "cwRemoteAccountModel.h"

class CAVEWHERE_LIB_EXPORT cwRemoteCredentialStore : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteCredentialStore)
    QML_UNCREATABLE("Access via RootData.remoteCredentialStore")

public:
    struct BlobReadResult {
        bool success = false;
        bool found = false;
        QByteArray value;
    };

    using BlobReadCallback = std::function<void(const BlobReadResult&)>;

    explicit cwRemoteCredentialStore(QObject* parent = nullptr);

    void writeCredentialBlob(cwRemoteAccountModel::Provider provider,
                             const QString& accountId,
                             const QByteArray& value,
                             QObject* context = nullptr);

    void deleteCredentialBlob(cwRemoteAccountModel::Provider provider,
                              const QString& accountId,
                              QObject* context = nullptr);

    void readCredentialBlob(cwRemoteAccountModel::Provider provider,
                            const QString& accountId,
                            QObject* context,
                            BlobReadCallback callback) const;

    static QString credentialBlobKey(cwRemoteAccountModel::Provider provider,
                                     const QString& accountId);

private:
    static QString keychainService();
};
