#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QQmlEngine>
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwRemoteAccountInfo
{
    Q_GADGET
    Q_PROPERTY(QString accountId MEMBER accountId)
    Q_PROPERTY(int provider MEMBER provider)
    Q_PROPERTY(QString username MEMBER username)
    Q_PROPERTY(int authState MEMBER authState)
    Q_PROPERTY(bool active MEMBER active)
    Q_PROPERTY(QDateTime lastUsedAt MEMBER lastUsedAt)
    Q_PROPERTY(QString displayName MEMBER displayName)

public:
    QString accountId;
    int provider = 0;
    QString username;
    int authState = 0;
    bool active = false;
    QDateTime lastUsedAt;
    QString displayName;
};
Q_DECLARE_METATYPE(cwRemoteAccountInfo)

class CAVEWHERE_LIB_EXPORT cwRemoteAccountModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteAccountModel)

public:
    enum class Provider {
        Unknown,
        GitHub,
        Bitbucket,
        GitLab
    };
    Q_ENUM(Provider)

    enum class AuthState {
        Unknown,
        Authorized,
        SignedOut,
        Revoked,
        Error
    };
    Q_ENUM(AuthState)

    enum Roles {
        ProviderRole = Qt::UserRole + 1,
        UsernameRole,
        LabelRole,
        AccountIdRole,
        AuthStateRole,
        ActiveRole,
        LastUsedAtRole
    };
    Q_ENUM(Roles)

    explicit cwRemoteAccountModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString upsertAccount(Provider provider, const QString& username);
    QString accountIdFor(Provider provider, const QString& username) const;
    void setAuthState(const QString& accountId, AuthState state);
    void setActiveAccount(Provider provider, const QString& accountId);
    Q_INVOKABLE QString activeAccountId(Provider provider) const;
    cwRemoteAccountInfo accountById(const QString& accountId) const;
    void removeAccountById(const QString& accountId);
    void setLastUsedAt(const QString& accountId, const QDateTime& value);

    // Legacy bridge API; will be removed in later phases.
    void addOrUpdateAccount(Provider provider, const QString& username);
    void removeAccount(Provider provider, const QString& username);
    void clear();

    static QString providerToString(Provider provider);
    static Provider providerFromString(const QString& str);
    static QString providerDisplayName(Provider provider);
    static QString authStateToString(AuthState state);
    static AuthState authStateFromString(const QString& str);

signals:
    void accountsChanged();

private:
    struct Entry {
        QString accountId;
        Provider provider = Provider::Unknown;
        QString username;
        AuthState authState = AuthState::Unknown;
        bool active = false;
        QDateTime lastUsedAt;
        QString displayName;
    };

    int findAccountById(const QString& accountId) const;
    int findAccount(Provider provider, const QString& username) const;
    void setAccountActiveByIndex(int targetIndex);
    void loadFromSettings();
    void saveToSettings() const;

    QList<Entry> m_entries;
};
