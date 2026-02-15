#include "cwGitHubLfsAuthProvider.h"

// Our
#include "cwGitHubIntegration.h"

cwGitHubLfsAuthProvider::cwGitHubLfsAuthProvider(cwGitHubIntegration* integration) :
    m_integration(integration)
{
    refreshCachedHeader();

    if (m_integration) {
        QObject::connect(m_integration, &cwGitHubIntegration::accessTokenChanged,
                         this, &cwGitHubLfsAuthProvider::refreshCachedHeader);
        QObject::connect(m_integration, &QObject::destroyed,
                         this, [this]() {
                             m_integration = nullptr;
                             refreshCachedHeader();
                         });
    }
}

QByteArray cwGitHubLfsAuthProvider::authorizationHeader(const QUrl& url) const
{
    if (!isGitHubHost(url.host())) {
        return {};
    }

    QReadLocker lock(&m_cachedHeaderLock);
    return m_cachedHeader;
}

void cwGitHubLfsAuthProvider::refreshCachedHeader()
{
    QByteArray header;
    if (m_integration) {
        header = m_integration->lfsAuthorizationHeader();
    }

    QWriteLocker lock(&m_cachedHeaderLock);
    m_cachedHeader = header;
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
