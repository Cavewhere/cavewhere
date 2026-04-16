/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#include "cwDeepLinkHandler.h"

//Qt includes
#include <QHash>
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
    QString error;
    QUrl repoUrl = validateAndExtractRepo(query, &error);
    if (repoUrl.isEmpty()) {
        emit invalidLink(error);
        return;
    }

    acceptRepoUrl(repoUrl);
}

void cwDeepLinkHandler::handleShareLink(const QUrl& url)
{
    if (url.scheme() == QLatin1String("cavewhere")) {
        handleUrl(url);
        return;
    }

    auto path = url.path();
    if (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }

    if (url.scheme() == QLatin1String("https")
        && url.host().toLower() == QLatin1String("cavewhere.com")
        && path == QLatin1String("/open")) {
        const QUrlQuery query(url);
        QString error;
        QUrl repoUrl = validateAndExtractRepo(query, &error);
        if (repoUrl.isEmpty()) {
            emit invalidLink(error);
            return;
        }
        acceptRepoUrl(repoUrl);
        return;
    }

    emit invalidLink(QStringLiteral(
        "Please paste a CaveWhere share link "
        "(https://cavewhere.com/open?repo=... or cavewhere://open?repo=...)"));
}

void cwDeepLinkHandler::acceptRepoUrl(const QUrl& repoUrl)
{
    m_pendingUrl = repoUrl;
    emit openRepoRequested(repoUrl);
}

QUrl cwDeepLinkHandler::validateAndExtractRepo(const QUrlQuery& query, QString* errorOut)
{
    if (!query.hasQueryItem(QStringLiteral("repo"))) {
        if (errorOut) { *errorOut = QStringLiteral("Missing 'repo' parameter"); }
        return {};
    }

    const QString repoStr = query.queryItemValue(QStringLiteral("repo"), QUrl::FullyDecoded);
    const QUrl repoUrl(repoStr);

    if (repoUrl.scheme() != QLatin1String("https")) {
        if (errorOut) { *errorOut = QStringLiteral("repo URL must use https:// scheme"); }
        return {};
    }

    const QString host = repoUrl.host();
    if (!isHostAllowed(host)) {
        if (errorOut) { *errorOut = QStringLiteral("repo host '%1' is not on the allowlist").arg(host); }
        return {};
    }

    static const QRegularExpression ipPattern(
        QStringLiteral(R"(^\d{1,3}(\.\d{1,3}){3}$)"));
    if (ipPattern.match(host).hasMatch()) {
        if (errorOut) { *errorOut = QStringLiteral("IP addresses are not allowed as repo host"); }
        return {};
    }

    if (repoUrl.path().contains(QLatin1String(".."))) {
        if (errorOut) { *errorOut = QStringLiteral("Path traversal detected in repo URL"); }
        return {};
    }

    return repoUrl;
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

bool cwDeepLinkHandler::isHostAllowed(const QString& host)
{
    return allowedHosts().contains(host.toLower());
}

QString cwDeepLinkHandler::hostDisplayName(const QUrl& url)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("github.com"),    QStringLiteral("GitHub")},
        {QStringLiteral("gitlab.com"),    QStringLiteral("GitLab")},
        {QStringLiteral("bitbucket.org"), QStringLiteral("Bitbucket")},
    };
    return names.value(url.host().toLower());
}

QUrl cwDeepLinkHandler::collaboratorSettingsUrl(const QUrl& repoUrl)
{
    if (!repoUrl.isValid() || repoUrl.scheme() != QStringLiteral("https"))
        return {};

    const QString host = repoUrl.host().toLower();
    QString path = repoUrl.path();

    // Strip trailing .git and slashes
    if (path.endsWith(QStringLiteral(".git")))
        path.chop(4);
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);

    if (host == QStringLiteral("github.com"))
        path += QStringLiteral("/settings/access");
    else if (host == QStringLiteral("gitlab.com"))
        path += QStringLiteral("/-/project_members");
    else if (host == QStringLiteral("bitbucket.org"))
        path += QStringLiteral("/admin/access-keys");
    else
        return {};

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(repoUrl.host());
    url.setPath(path);
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
