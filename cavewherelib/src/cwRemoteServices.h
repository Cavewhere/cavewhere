#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>

#include "CaveWhereLibExport.h"

class cwGitHubIntegration;
class cwRemoteAccountModel;
class cwRemoteCredentialStore;
class cwRemoteBindingStore;
class cwRemoteAccountCoordinator;

class CAVEWHERE_LIB_EXPORT cwRemoteServices : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteServices)
    QML_UNCREATABLE("Access via RootData.remote")

    Q_PROPERTY(cwGitHubIntegration* gitHubIntegration READ gitHubIntegration CONSTANT)
    Q_PROPERTY(cwRemoteAccountModel* accountModel READ accountModel CONSTANT)
    Q_PROPERTY(cwRemoteCredentialStore* credentialStore READ credentialStore CONSTANT)
    Q_PROPERTY(cwRemoteBindingStore* bindingStore READ bindingStore CONSTANT)
    Q_PROPERTY(cwRemoteAccountCoordinator* accountCoordinator READ accountCoordinator CONSTANT)

public:
    explicit cwRemoteServices(QObject* parent = nullptr);

    cwGitHubIntegration* gitHubIntegration() const;
    cwRemoteAccountModel* accountModel() const;
    cwRemoteCredentialStore* credentialStore() const;
    cwRemoteBindingStore* bindingStore() const;
    cwRemoteAccountCoordinator* accountCoordinator() const;

private:
    void handleLfsAuthenticationFailed(const QUrl& url, int httpStatus, const QString& message);

    mutable cwGitHubIntegration* m_gitHubIntegration = nullptr;
    mutable cwRemoteAccountModel* m_accountModel = nullptr;
    mutable cwRemoteCredentialStore* m_credentialStore = nullptr;
    mutable cwRemoteBindingStore* m_bindingStore = nullptr;
    mutable cwRemoteAccountCoordinator* m_accountCoordinator = nullptr;
};
