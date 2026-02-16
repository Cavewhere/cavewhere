#pragma once

// Qt
#include <QObject>
#include <QPointer>
#include <QReadWriteLock>
#include <QByteArray>
#include <QUrl>
#include <QHash>
#include <QSet>

// QQuickGit
#include "LfsAuthProvider.h"

class cwGitHubIntegration;
class cwRemoteAccountModel;
class cwRemoteBindingStore;
class cwRemoteCredentialStore;

class cwGitHubLfsAuthProvider : public QObject, public QQuickGit::LfsAuthProvider
{
    Q_OBJECT

public:
    explicit cwGitHubLfsAuthProvider(cwGitHubIntegration* integration,
                                     cwRemoteAccountModel* accountModel,
                                     cwRemoteBindingStore* bindingStore,
                                     cwRemoteCredentialStore* credentialStore);

    QByteArray authorizationHeader(const QUrl& url) const override;

private:
    QByteArray resolveHeaderForAccount(const QString& accountId) const;
    QString resolvePreferredAccountId(const QUrl& url) const;
    QString activeGitHubAccountId() const;
    bool isAuthorizedAccount(const QString& accountId) const;
    void queueCredentialLoad(const QString& accountId) const;
    void refreshActiveAccountHeaderCache();
    static QByteArray lfsHeaderForToken(const QString& token);
    static bool isGitHubHost(const QString& host);

    QPointer<cwGitHubIntegration> m_integration;
    QPointer<cwRemoteAccountModel> m_accountModel;
    QPointer<cwRemoteBindingStore> m_bindingStore;
    QPointer<cwRemoteCredentialStore> m_credentialStore;
    mutable QReadWriteLock m_headerByAccountLock;
    mutable QHash<QString, QByteArray> m_headerByAccountId;
    mutable QSet<QString> m_inFlightLoads;
};
