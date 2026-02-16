#include "cwRemoteAccountCoordinator.h"
#include "cwRemoteBindingStore.h"

cwRemoteAccountCoordinator::cwRemoteAccountCoordinator(cwGitHubIntegration* gitHubIntegration,
                                                       cwRemoteAccountModel* remoteAccountModel,
                                                       cwRemoteBindingStore* remoteBindingStore,
                                                       QObject* parent) :
    QObject(parent),
    m_gitHubIntegration(gitHubIntegration),
    m_remoteAccountModel(remoteAccountModel),
    m_remoteBindingStore(remoteBindingStore)
{
    if (m_gitHubIntegration) {
        connect(m_gitHubIntegration, &cwGitHubIntegration::authorizationSucceeded,
                this, &cwRemoteAccountCoordinator::syncAuthorizedGitHubAccount);
        connect(m_gitHubIntegration, &cwGitHubIntegration::profileResolved,
                this, &cwRemoteAccountCoordinator::syncAuthorizedGitHubAccount);
        connect(m_gitHubIntegration, &cwGitHubIntegration::loggedOut,
                this, &cwRemoteAccountCoordinator::handleGitHubLoggedOut);
        connect(m_gitHubIntegration, &cwGitHubIntegration::tokenInvalidated,
                this, &cwRemoteAccountCoordinator::handleGitHubTokenInvalidated);
    }

    bootstrapGitHubActiveAccount();
}

void cwRemoteAccountCoordinator::forgetGitHubAccount()
{
    QString username;
    if (m_gitHubIntegration) {
        username = m_gitHubIntegration->username();
    }
    forgetGitHubAccount(username);
}

void cwRemoteAccountCoordinator::forgetGitHubAccount(const QString& username)
{
    QString usernameToRemove = username.trimmed();
    QString accountId;
    if (m_gitHubIntegration) {
        accountId = m_gitHubIntegration->activeAccountId().trimmed();
    }

    if (accountId.isEmpty() && usernameToRemove.isEmpty() && m_gitHubIntegration) {
        usernameToRemove = m_gitHubIntegration->username().trimmed();
    }

    if (accountId.isEmpty() && m_remoteAccountModel && !usernameToRemove.isEmpty()) {
        accountId = m_remoteAccountModel->accountIdFor(cwRemoteAccountModel::Provider::GitHub, usernameToRemove);
    }

    if (accountId.isEmpty()) {
        accountId = initialGitHubAccountId();
    }

    if (m_gitHubIntegration) {
        if (!accountId.isEmpty()) {
            m_gitHubIntegration->setActiveAccountId(accountId);
        }
        m_gitHubIntegration->logout();
    }

    if (m_remoteAccountModel && !accountId.isEmpty()) {
        m_remoteAccountModel->removeAccountById(accountId);
    }
    if (m_remoteBindingStore && !accountId.isEmpty()) {
        m_remoteBindingStore->removeBindingsForAccount(accountId);
    }
}

void cwRemoteAccountCoordinator::startAddGitHubAccount()
{
    if (!m_gitHubIntegration) {
        return;
    }
    m_gitHubIntegration->startDeviceLogin();
}

void cwRemoteAccountCoordinator::selectGitHubAccount(const QString& username)
{
    if (!m_remoteAccountModel || !m_gitHubIntegration) {
        return;
    }

    const QString accountId = m_remoteAccountModel->accountIdFor(cwRemoteAccountModel::Provider::GitHub, username);
    if (accountId.isEmpty()) {
        return;
    }

    m_remoteAccountModel->setActiveAccount(cwRemoteAccountModel::Provider::GitHub, accountId);
    m_gitHubIntegration->setActiveAccountId(accountId);
}

void cwRemoteAccountCoordinator::bindRemoteToActiveGitHubAccount(const QString& remoteUrl)
{
    if (!m_gitHubIntegration || !m_remoteBindingStore) {
        return;
    }

    const QString accountId = m_gitHubIntegration->activeAccountId().trimmed();
    if (accountId.isEmpty()) {
        return;
    }

    m_remoteBindingStore->bindRemoteToAccount(remoteUrl, accountId);
}

void cwRemoteAccountCoordinator::syncAuthorizedGitHubAccount()
{
    if (!m_gitHubIntegration || !m_remoteAccountModel) {
        return;
    }

    if (m_gitHubIntegration->authState() != cwGitHubIntegration::AuthState::Authorized) {
        return;
    }

    const QString username = m_gitHubIntegration->username().trimmed();
    if (username.isEmpty()) {
        return;
    }

    const QString accountId = m_remoteAccountModel->upsertAccount(cwRemoteAccountModel::Provider::GitHub, username);
    if (accountId.isEmpty()) {
        return;
    }

    m_gitHubIntegration->setActiveAccountId(accountId);
    m_gitHubIntegration->persistCurrentAccessTokenForAccount(accountId);
    m_remoteAccountModel->setAuthState(accountId, cwRemoteAccountModel::AuthState::Authorized);
    m_remoteAccountModel->setActiveAccount(cwRemoteAccountModel::Provider::GitHub, accountId);
}

QString cwRemoteAccountCoordinator::initialGitHubAccountId() const
{
    if (!m_remoteAccountModel) {
        return {};
    }

    const QString activeId = m_remoteAccountModel->activeAccountId(cwRemoteAccountModel::Provider::GitHub);
    if (!activeId.isEmpty()) {
        return activeId;
    }

    for (int row = 0; row < m_remoteAccountModel->rowCount(); ++row) {
        const QModelIndex index = m_remoteAccountModel->index(row, 0);
        const auto provider = static_cast<cwRemoteAccountModel::Provider>(
            m_remoteAccountModel->data(index, cwRemoteAccountModel::ProviderRole).toInt());
        if (provider != cwRemoteAccountModel::Provider::GitHub) {
            continue;
        }
        return m_remoteAccountModel->data(index, cwRemoteAccountModel::AccountIdRole).toString();
    }

    return {};
}

void cwRemoteAccountCoordinator::bootstrapGitHubActiveAccount()
{
    if (!m_gitHubIntegration) {
        return;
    }

    const QString accountId = initialGitHubAccountId();
    if (!accountId.isEmpty()) {
        m_gitHubIntegration->setActiveAccountId(accountId);
    }
}

void cwRemoteAccountCoordinator::handleGitHubLoggedOut(const QString& accountId)
{
    if (!m_remoteAccountModel || accountId.isEmpty()) {
        return;
    }

    m_remoteAccountModel->setAuthState(accountId, cwRemoteAccountModel::AuthState::SignedOut);
}

void cwRemoteAccountCoordinator::handleGitHubTokenInvalidated(const QString& accountId, const QString& message)
{
    Q_UNUSED(message)

    if (!m_remoteAccountModel) {
        return;
    }

    const QString normalizedId = accountId.trimmed();
    if (!normalizedId.isEmpty()) {
        m_remoteAccountModel->setAuthState(normalizedId, cwRemoteAccountModel::AuthState::Revoked);
    }

    if (!m_gitHubIntegration) {
        return;
    }

    const QString fallbackAccountId = firstAuthorizedGitHubAccountExcluding(normalizedId);
    if (fallbackAccountId.isEmpty()) {
        return;
    }

    m_remoteAccountModel->setActiveAccount(cwRemoteAccountModel::Provider::GitHub, fallbackAccountId);
    m_gitHubIntegration->setActiveAccountId(fallbackAccountId);
}

QString cwRemoteAccountCoordinator::firstAuthorizedGitHubAccountExcluding(const QString& accountId) const
{
    if (!m_remoteAccountModel) {
        return {};
    }

    const QString excludedId = accountId.trimmed();
    for (int row = 0; row < m_remoteAccountModel->rowCount(); ++row) {
        const QModelIndex index = m_remoteAccountModel->index(row, 0);
        const auto provider = static_cast<cwRemoteAccountModel::Provider>(
            m_remoteAccountModel->data(index, cwRemoteAccountModel::ProviderRole).toInt());
        if (provider != cwRemoteAccountModel::Provider::GitHub) {
            continue;
        }

        const QString candidateId = m_remoteAccountModel->data(index, cwRemoteAccountModel::AccountIdRole).toString().trimmed();
        if (candidateId.isEmpty() || candidateId == excludedId) {
            continue;
        }

        const auto authState = static_cast<cwRemoteAccountModel::AuthState>(
            m_remoteAccountModel->data(index, cwRemoteAccountModel::AuthStateRole).toInt());
        if (authState == cwRemoteAccountModel::AuthState::Authorized) {
            return candidateId;
        }
    }

    return {};
}
