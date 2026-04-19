/**************************************************************************
**
**    Copyright (C) 2026 by CaveWhere
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDEEPLINKHANDLER_H
#define CWDEEPLINKHANDLER_H

//Qt includes
#include <QObject>
#include <QUrl>
#include <QUrlQuery>
#include <QtQml/qqmlregistration.h>

//Our includes
#include "cwGlobals.h"

/**
 * @brief Parses and validates incoming cavewhere:// deep-link URLs.
 *
 * Expected URL format: cavewhere://open?repo=<percent-encoded-https-url>
 *
 * Validation rules:
 *  - repo param must be present
 *  - repo param must use https:// scheme
 *  - repo host must be on the allowlist (github.com, gitlab.com, bitbucket.org)
 *
 * Startup race (Windows): the URL may arrive via argv before QML is loaded.
 * handleUrl() always emits openRepoRequested (no-op if nothing is connected yet)
 * and stores the validated repo URL as a pending URL. CavewhereMainWindow.qml's
 * Component.onCompleted calls takePendingUrl() to drain any URL that arrived
 * before the signal connection was established.
 */
class CAVEWHERE_LIB_EXPORT cwDeepLinkHandler : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DeepLinkHandler)

public:
    explicit cwDeepLinkHandler(QObject* parent = nullptr);

    Q_INVOKABLE void handleUrl(const QUrl& url);
    Q_INVOKABLE void handleShareLink(const QString& text);
    Q_INVOKABLE QString takePendingUrl();

    static bool isHostAllowed(const QString& host);
    Q_INVOKABLE static QString hostDisplayName(const QUrl& url);
    Q_INVOKABLE static QUrl collaboratorSettingsUrl(const QUrl& repoUrl);

signals:
    void openRepoRequested(QString repoUrl);
    void invalidLink(QString reason);

private:
    QUrl validateAndExtractRepo(const QUrlQuery& query, QString* errorOut);
    void acceptRepoUrl(const QString& repoUrl);
    static bool looksLikeCloneUrl(const QString& text);

    QString m_pendingUrl;

    static const QStringList& allowedHosts();
};

#endif // CWDEEPLINKHANDLER_H
