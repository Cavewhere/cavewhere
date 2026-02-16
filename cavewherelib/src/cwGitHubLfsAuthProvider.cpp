#include "cwGitHubLfsAuthProvider.h"

// Our
#include "cwGitHubIntegration.h"
#include "cwRemoteAccountModel.h"
#include "cwRemoteBindingStore.h"
#include "cwRemoteCredentialStore.h"

cwGitHubLfsAuthProvider::cwGitHubLfsAuthProvider(cwGitHubIntegration* integration,
                                                 cwRemoteAccountModel* accountModel,
                                                 cwRemoteBindingStore* bindingStore,
                                                 cwRemoteCredentialStore* credentialStore) :
    m_integration(integration),
    m_accountModel(accountModel),
    m_bindingStore(bindingStore),
    m_credentialStore(credentialStore)
{
    refreshActiveAccountHeaderCache();

    if (m_integration) {
        QObject::connect(m_integration, &cwGitHubIntegration::accessTokenChanged,
                         this, &cwGitHubLfsAuthProvider::refreshActiveAccountHeaderCache);
        QObject::connect(m_integration, &cwGitHubIntegration::activeAccountIdChanged,
                         this, &cwGitHubLfsAuthProvider::refreshActiveAccountHeaderCache);
        QObject::connect(m_integration, &cwGitHubIntegration::loggedOut,
                         this, [this](const QString& accountId) {
                             QWriteLocker locker(&m_headerByAccountLock);
                             m_headerByAccountId.remove(accountId.trimmed());
                             m_inFlightLoads.remove(accountId.trimmed());
                         });
        QObject::connect(m_integration, &QObject::destroyed,
                         this, [this]() {
                             m_integration = nullptr;
                             QWriteLocker locker(&m_headerByAccountLock);
                             m_headerByAccountId.clear();
                             m_inFlightLoads.clear();
                         });
    }
}

QByteArray cwGitHubLfsAuthProvider::authorizationHeader(const QUrl& url) const
{
    if (!isGitHubHost(url.host())) {
        return {};
    }

    const QString preferredAccountId = resolvePreferredAccountId(url);
    if (!preferredAccountId.isEmpty()) {
        return resolveHeaderForAccount(preferredAccountId);
    }

    return {};
}

QByteArray cwGitHubLfsAuthProvider::resolveHeaderForAccount(const QString& accountId) const
{
    const QString normalized = accountId.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    if (m_integration && m_integration->activeAccountId().trimmed() == normalized) {
        const QByteArray liveHeader = m_integration->lfsAuthorizationHeader();
        if (!liveHeader.isEmpty()) {
            QWriteLocker locker(&m_headerByAccountLock);
            m_headerByAccountId.insert(normalized, liveHeader);
            return liveHeader;
        }
    }

    {
        QReadLocker locker(&m_headerByAccountLock);
        const QByteArray cached = m_headerByAccountId.value(normalized);
        if (!cached.isEmpty()) {
            return cached;
        }
    }

    queueCredentialLoad(normalized);
    return {};
}

QString cwGitHubLfsAuthProvider::resolvePreferredAccountId(const QUrl& url) const
{
    const QString boundAccountId = (m_bindingStore != nullptr)
        ? m_bindingStore->accountIdForRemote(url.toString())
        : QString();

    if (!boundAccountId.isEmpty() && isAuthorizedAccount(boundAccountId)) {
        return boundAccountId;
    }

    const QString activeAccount = activeGitHubAccountId();
    if (!activeAccount.isEmpty() && isAuthorizedAccount(activeAccount)) {
        return activeAccount;
    }

    return {};
}

QString cwGitHubLfsAuthProvider::activeGitHubAccountId() const
{
    if (m_accountModel) {
        const QString active = m_accountModel->activeAccountId(cwRemoteAccountModel::Provider::GitHub).trimmed();
        if (!active.isEmpty()) {
            return active;
        }
    }

    if (m_integration) {
        return m_integration->activeAccountId().trimmed();
    }

    return {};
}

bool cwGitHubLfsAuthProvider::isAuthorizedAccount(const QString& accountId) const
{
    if (!m_accountModel) {
        return false;
    }

    const QString normalized = accountId.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    for (int row = 0; row < m_accountModel->rowCount(); ++row) {
        const QModelIndex index = m_accountModel->index(row, 0);
        if (!index.isValid()) {
            continue;
        }

        const QString candidateId = m_accountModel->data(index, cwRemoteAccountModel::AccountIdRole).toString().trimmed();
        if (candidateId != normalized) {
            continue;
        }

        const auto authState = static_cast<cwRemoteAccountModel::AuthState>(
            m_accountModel->data(index, cwRemoteAccountModel::AuthStateRole).toInt());
        return authState == cwRemoteAccountModel::AuthState::Authorized;
    }

    return false;
}

void cwGitHubLfsAuthProvider::queueCredentialLoad(const QString& accountId) const
{
    const QString normalized = accountId.trimmed();
    if (!m_credentialStore || normalized.isEmpty()) {
        return;
    }

    {
        QWriteLocker locker(&m_headerByAccountLock);
        if (m_inFlightLoads.contains(normalized)) {
            return;
        }
        m_inFlightLoads.insert(normalized);
    }

    m_credentialStore->readAccessToken(cwRemoteAccountModel::Provider::GitHub,
                                       normalized,
                                       const_cast<cwGitHubLfsAuthProvider*>(this),
                                       [this, normalized](const cwRemoteCredentialStore::ReadResult& result) {
        const QByteArray loadedHeader = (result.success && result.found)
            ? lfsHeaderForToken(result.value)
            : QByteArray();

        QWriteLocker locker(&m_headerByAccountLock);
        m_inFlightLoads.remove(normalized);
        if (loadedHeader.isEmpty()) {
            m_headerByAccountId.remove(normalized);
            return;
        }

        m_headerByAccountId.insert(normalized, loadedHeader);
    });
}

void cwGitHubLfsAuthProvider::refreshActiveAccountHeaderCache()
{
    const QString activeId = activeGitHubAccountId();
    if (activeId.isEmpty()) {
        return;
    }

    const QByteArray liveHeader = (m_integration != nullptr)
        ? m_integration->lfsAuthorizationHeader()
        : QByteArray();
    if (liveHeader.isEmpty()) {
        return;
    }

    QWriteLocker locker(&m_headerByAccountLock);
    m_headerByAccountId.insert(activeId, liveHeader);
}

QByteArray cwGitHubLfsAuthProvider::lfsHeaderForToken(const QString& token)
{
    if (token.isEmpty()) {
        return {};
    }

    const QByteArray credentials = QByteArrayLiteral("x-access-token:") + token.toUtf8();
    return QByteArrayLiteral("Basic ") + credentials.toBase64();
}

bool cwGitHubLfsAuthProvider::isGitHubHost(const QString& host)
{
    const QString lowered = host.trimmed().toLower();
    if (lowered.isEmpty()) {
        return false;
    }

    return lowered == QStringLiteral("github.com")
           || lowered == QStringLiteral("api.github.com")
           || lowered.endsWith(QStringLiteral(".github.com"));
}
