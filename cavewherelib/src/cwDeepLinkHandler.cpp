/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#include "cwDeepLinkHandler.h"

//Qt includes
#include <QUrlQuery>
#include <QRegularExpression>

cwDeepLinkHandler::cwDeepLinkHandler(QObject* parent)
    : QObject(parent)
{
}

void cwDeepLinkHandler::handleUrl(const QUrl& url)
{
    if (url.scheme() != QLatin1String("cavewhere")) {
        emit invalidLink(QStringLiteral("Not a cavewhere:// URL"));
        return;
    }

    if (url.host() != QLatin1String("open")) {
        emit invalidLink(QStringLiteral("Unknown cavewhere:// path: expected cavewhere://open"));
        return;
    }

    const QUrlQuery query(url);
    if (!query.hasQueryItem(QStringLiteral("repo"))) {
        emit invalidLink(QStringLiteral("Missing 'repo' parameter"));
        return;
    }

    const QString repoStr = query.queryItemValue(QStringLiteral("repo"), QUrl::FullyDecoded);
    const QUrl repoUrl(repoStr);

    if (repoUrl.scheme() != QLatin1String("https")) {
        emit invalidLink(QStringLiteral("repo URL must use https:// scheme"));
        return;
    }

    const QString host = repoUrl.host().toLower();
    if (!allowedHosts().contains(host)) {
        emit invalidLink(QStringLiteral("repo host '%1' is not on the allowlist").arg(host));
        return;
    }

    static const QRegularExpression ipPattern(
        QStringLiteral(R"(^\d{1,3}(\.\d{1,3}){3}$)"));
    if (ipPattern.match(host).hasMatch()) {
        emit invalidLink(QStringLiteral("IP addresses are not allowed as repo host"));
        return;
    }

    if (repoUrl.path().contains(QLatin1String(".."))) {
        emit invalidLink(QStringLiteral("Path traversal detected in repo URL"));
        return;
    }

    m_pendingUrl = repoUrl;
    emit openRepoRequested(repoUrl);
}

/**
 * @brief Returns and clears any URL that arrived before QML was ready.
 *
 * Call from CavewhereMainWindow.qml Component.onCompleted to handle the
 * Windows startup case where argv delivers the URL before the signal
 * connection to the confirmation dialog exists.
 */
QUrl cwDeepLinkHandler::takePendingUrl()
{
    QUrl url = m_pendingUrl;
    m_pendingUrl.clear();
    return url;
}

const QStringList& cwDeepLinkHandler::allowedHosts()
{
    static const QStringList hosts = {
        QStringLiteral("github.com"),
        QStringLiteral("gitlab.com"),
        QStringLiteral("bitbucket.org"),
    };
    return hosts;
}
