#pragma once

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
    struct ReadResult {
        bool success = false;
        bool found = false;
        QString value;
    };

    using ReadCallback = std::function<void(const ReadResult&)>;

    explicit cwRemoteCredentialStore(QObject* parent = nullptr);

    void writeAccessToken(cwRemoteAccountModel::Provider provider,
                          const QString& accountId,
                          const QString& token,
                          QObject* context = nullptr);

    void deleteAccessToken(cwRemoteAccountModel::Provider provider,
                           const QString& accountId,
                           QObject* context = nullptr);

    void readAccessToken(cwRemoteAccountModel::Provider provider,
                         const QString& accountId,
                         QObject* context,
                         ReadCallback callback) const;

    static QString accessTokenKey(cwRemoteAccountModel::Provider provider, const QString& accountId);

private:
    static QString keychainService();
};
