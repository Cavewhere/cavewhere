// cwDiscordStatusChecker.h

#ifndef CWDISCORDSTATUSCHECKER_H
#define CWDISCORDSTATUSCHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QProperty>
#include <QQmlEngine>

class cwDiscordStatusChecker : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(DiscordStatusChecker)

    Q_PROPERTY(int devsOnlineCount READ devsOnlineCount BINDABLE bindableDevsOnlineCount)
    Q_PROPERTY(int userCount READ userCount BINDABLE bindableUserCount)
    Q_PROPERTY(QUrl server READ server CONSTANT);

public:
    explicit cwDiscordStatusChecker(QObject *parent = nullptr);

    int devsOnlineCount() const;
    QBindable<int> bindableDevsOnlineCount();

    int userCount() const;
    QBindable<int> bindableUserCount();

    QUrl server() const { return QUrl(QStringLiteral("https://discord.gg/SGm5Px6phT")); }

private slots:
    void fetchDiscordStatus();
    void onNetworkReplyFinished(QNetworkReply *reply);

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwDiscordStatusChecker, int, m_devsOnlineCount, 0)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwDiscordStatusChecker, int, m_userCount, 0)

    QNetworkAccessManager *m_networkManager;
    QTimer *m_timer;

    QStringList m_devUsernames; // List of developer usernames to check
};

inline int cwDiscordStatusChecker::devsOnlineCount() const
{
    return m_devsOnlineCount;
}

inline int cwDiscordStatusChecker::userCount() const
{
    return m_userCount;
}

inline QBindable<int> cwDiscordStatusChecker::bindableDevsOnlineCount()
{
    return QBindable<int>(&m_devsOnlineCount);
}

inline QBindable<int> cwDiscordStatusChecker::bindableUserCount()
{
    return QBindable<int>(&m_userCount);
}

#endif // CWDISCORDSTATUSCHECKER_H
