#include "cwRemoteServices.h"

#include "cwGitHubIntegration.h"
#include "cwGitHubLfsAuthProvider.h"
#include "cwRemoteAccountModel.h"
#include "cwRemoteCredentialStore.h"
#include "cwRemoteBindingStore.h"
#include "cwRemoteAccountCoordinator.h"
#include "LfsBatchClient.h"
#include "LfsAuthFailureNotifier.h"

#include <memory>

cwRemoteServices::cwRemoteServices(QObject* parent)
    : QObject(parent)
{
    QQuickGit::LfsBatchClient::setLfsAuthProvider(
        std::make_shared<cwGitHubLfsAuthProvider>(gitHubIntegration(),
                                                  accountModel(),
                                                  bindingStore(),
                                                  credentialStore()));

    QObject::connect(QQuickGit::LfsBatchClient::authFailureNotifier(),
                     &QQuickGit::LfsAuthFailureNotifier::authenticationFailed,
                     this,
                     &cwRemoteServices::handleLfsAuthenticationFailed);
}

cwGitHubIntegration* cwRemoteServices::gitHubIntegration() const
{
    if (!m_gitHubIntegration) {
        m_gitHubIntegration = new cwGitHubIntegration(credentialStore(),
                                                      const_cast<cwRemoteServices*>(this));
    }
    return m_gitHubIntegration;
}

cwRemoteAccountModel* cwRemoteServices::accountModel() const
{
    if (!m_accountModel) {
        m_accountModel = new cwRemoteAccountModel(const_cast<cwRemoteServices*>(this));
    }
    return m_accountModel;
}

cwRemoteCredentialStore* cwRemoteServices::credentialStore() const
{
    if (!m_credentialStore) {
        m_credentialStore = new cwRemoteCredentialStore(const_cast<cwRemoteServices*>(this));
    }
    return m_credentialStore;
}

cwRemoteBindingStore* cwRemoteServices::bindingStore() const
{
    if (!m_bindingStore) {
        m_bindingStore = new cwRemoteBindingStore(const_cast<cwRemoteServices*>(this));
    }
    return m_bindingStore;
}

cwRemoteAccountCoordinator* cwRemoteServices::accountCoordinator() const
{
    if (!m_accountCoordinator) {
        m_accountCoordinator = new cwRemoteAccountCoordinator(gitHubIntegration(),
                                                              accountModel(),
                                                              bindingStore(),
                                                              const_cast<cwRemoteServices*>(this));
    }
    return m_accountCoordinator;
}

void cwRemoteServices::handleLfsAuthenticationFailed(const QUrl& url,
                                                     int httpStatus,
                                                     const QString& message)
{
    if (!accountCoordinator()) {
        return;
    }

    accountCoordinator()->handleGitHubLfsAuthFailure(url, httpStatus, message);
}
