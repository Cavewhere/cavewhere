#pragma once

// Qt
#include <QObject>
#include <QPointer>
#include <QReadWriteLock>
#include <QByteArray>
#include <QUrl>

// QQuickGit
#include "LfsAuthProvider.h"

class cwGitHubIntegration;

class cwGitHubLfsAuthProvider : public QObject, public QQuickGit::LfsAuthProvider
{
    Q_OBJECT

public:
    explicit cwGitHubLfsAuthProvider(cwGitHubIntegration* integration);

    QByteArray authorizationHeader(const QUrl& url) const override;

private:
    void refreshCachedHeader();
    static bool isGitHubHost(const QString& host);

    QPointer<cwGitHubIntegration> m_integration;
    mutable QReadWriteLock m_cachedHeaderLock;
    QByteArray m_cachedHeader;
};
