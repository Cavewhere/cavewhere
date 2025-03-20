// cwDiscordStatusChecker.cpp

#include "cwDiscordStatusChecker.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

cwDiscordStatusChecker::cwDiscordStatusChecker(QObject *parent)
    : QObject(parent),
    m_networkManager(new QNetworkAccessManager(this)),
    m_timer(new QTimer(this)),
    m_devsOnlineCount(0),
    m_userCount(0),
    m_devUsernames({"⛰ vpicaver ⛰"})
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &cwDiscordStatusChecker::onNetworkReplyFinished);

    connect(m_timer, &QTimer::timeout, this, &cwDiscordStatusChecker::fetchDiscordStatus);

    const int minutes = 5;
    const int minutesToMs = 60 * 1000;
    const int ms = minutes * minutesToMs;

    m_timer->start(ms); // Refresh every 5 minutes

    // Initial fetch
    fetchDiscordStatus();
}


void cwDiscordStatusChecker::fetchDiscordStatus()
{
    QUrl url("https://discord.com/api/guilds/1217210007443996764/widget.json");
    QNetworkRequest request(url);
    m_networkManager->get(request);
}

void cwDiscordStatusChecker::onNetworkReplyFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonObject jsonObj = jsonDoc.object();

        int onlineCount = jsonObj["presence_count"].toInt();
        QJsonArray membersArray = jsonObj["members"].toArray();

        int devsOnline = 0;
        for (const QJsonValue &memberValue : membersArray) {
            QJsonObject memberObj = memberValue.toObject();
            QString username = memberObj["username"].toString();
            if (m_devUsernames.contains(username)) {
                devsOnline++;
            }
        }

        // Update properties
        m_userCount = onlineCount;
        m_devsOnlineCount = devsOnline;

    } else {
        qDebug() << "Error fetching Discord status:" << reply->errorString();
    }
    reply->deleteLater();
}
