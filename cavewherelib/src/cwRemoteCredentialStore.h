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

    void writeRefreshToken(cwRemoteAccountModel::Provider provider,
                           const QString& accountId,
                           const QString& token,
                           QObject* context = nullptr);

    void deleteRefreshToken(cwRemoteAccountModel::Provider provider,
                            const QString& accountId,
                            QObject* context = nullptr);

    void readRefreshToken(cwRemoteAccountModel::Provider provider,
                          const QString& accountId,
                          QObject* context,
                          ReadCallback callback) const;

    // Absolute epoch seconds (UTC). -1 means "not set".
    void writeAccessTokenExpiresAt(cwRemoteAccountModel::Provider provider,
                                   const QString& accountId,
                                   qint64 epochSeconds,
                                   QObject* context = nullptr);

    void deleteAccessTokenExpiresAt(cwRemoteAccountModel::Provider provider,
                                    const QString& accountId,
                                    QObject* context = nullptr);

    void readAccessTokenExpiresAt(cwRemoteAccountModel::Provider provider,
                                  const QString& accountId,
                                  QObject* context,
                                  std::function<void(bool success, bool found, qint64 epochSeconds)> callback) const;

    static QString accessTokenKey(cwRemoteAccountModel::Provider provider, const QString& accountId);
    static QString refreshTokenKey(cwRemoteAccountModel::Provider provider, const QString& accountId);
    static QString accessTokenExpiresAtKey(cwRemoteAccountModel::Provider provider, const QString& accountId);

private:
    void writeValue(const QString& key, const QString& value, QObject* context);
    void deleteValue(const QString& key, QObject* context);
    void readValue(const QString& key, QObject* context, ReadCallback callback) const;

    static QString keychainService();
    static QString credentialKey(cwRemoteAccountModel::Provider provider,
                                 const QString& accountId,
                                 QLatin1StringView field);
};
