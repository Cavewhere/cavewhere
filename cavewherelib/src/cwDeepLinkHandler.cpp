/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#include "cwDeepLinkHandler.h"

#include "cwGitHostingProvider.h"

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
    QString error;
    QUrl repoUrl = validateAndExtractRepo(query, &error);
    if (repoUrl.isEmpty()) {
        emit invalidLink(error);
        return;
    }

    acceptRepoUrl(repoUrl.toString());
}

void cwDeepLinkHandler::handleShareLink(const QString& text)
{
    const QString trimmed = text.trimmed();
    const QUrl url(trimmed);
    if (url.scheme() == QLatin1String("cavewhere")) {
        handleUrl(url);
        return;
    }

    QString path = url.path();
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
        acceptRepoUrl(repoUrl.toString());
        return;
    }

    // Forward the raw text so SSH shapes like `git@host:user/repo.git`
    // survive — QUrl parses those to an empty string.
    if (looksLikeCloneUrl(trimmed)) {
        acceptRepoUrl(trimmed);
        return;
    }

    emit invalidLink(QStringLiteral(
        "Please paste a CaveWhere share link or repository URL "
        "(e.g. https://cavewhere.com/open?repo=..., cavewhere://open?repo=..., "
        "or https://github.com/user/repo.git)"));
}

bool cwDeepLinkHandler::looksLikeCloneUrl(const QString& text)
{
    if (text.isEmpty()) {
        return false;
    }

    if (text.contains(QLatin1String("://"))) {
        return true;
    }

    const int atIndex = text.indexOf(QLatin1Char('@'));
    const int colonIndex = text.lastIndexOf(QLatin1Char(':'));
    if (atIndex != -1 && colonIndex > atIndex) {
        return true;
    }

    return text.contains(QLatin1Char('/'));
}

void cwDeepLinkHandler::acceptRepoUrl(const QString& repoUrl)
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
QString cwDeepLinkHandler::takePendingUrl()
{
    QString url = m_pendingUrl;
    m_pendingUrl.clear();
    return url;
}

bool cwDeepLinkHandler::isHostAllowed(const QString& host)
{
    return allowedHosts().contains(host.toLower());
}

QString cwDeepLinkHandler::hostDisplayName(const QUrl& url)
{
    return cwGitHostingProvider::forUrl(url).displayName;
}

QUrl cwDeepLinkHandler::collaboratorSettingsUrl(const QUrl& repoUrl)
{
    if (!repoUrl.isValid() || repoUrl.scheme() != QStringLiteral("https")) {
        return {};
    }
    return cwGitHostingProvider::collaboratorSettingsUrl(
        cwGitHostingProvider::forUrl(repoUrl), repoUrl);
}

const QStringList& cwDeepLinkHandler::allowedHosts()
{
    return cwGitHostingProvider::knownHosts();
}
