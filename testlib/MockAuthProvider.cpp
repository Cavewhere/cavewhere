#include "MockAuthProvider.h"

MockAuthProvider::MockAuthProvider(QObject* parent)
    : cwRemoteAuthProvider(parent)
{
}

void MockAuthProvider::completeLoad(const QString& token)
{
    m_token = token;
    m_hasLoaded = true;
    if (!token.isEmpty()) {
        emit accessTokenChanged();
    }
    emit credentialsLoaded();
}

void MockAuthProvider::setToken(const QString& token)
{
    m_token = token;
    emit accessTokenChanged();
}

void MockAuthProvider::verifyInstallation()
{
    ++m_verifyCalls;
    const bool installed = m_installed;
    QMetaObject::invokeMethod(this, [this, installed]() {
        emit installationVerified(installed);
    }, Qt::QueuedConnection);
}
