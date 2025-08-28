#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QTimer>

class cwGitHubDeviceAuth : public QObject {
    Q_OBJECT

public:
    struct DeviceCodeInfo {
        QString deviceCode;
        QString userCode;
        QUrl verificationWebAddress;
        int expiresInSeconds = 0;
        int pollIntervalSeconds = 5;
    };

    struct AccessTokenResult {
        bool success = false;
        QString accessToken;
        QString tokenType;
        QString scope;
        QString errorName;
        QString errorDescription;
    };

    explicit cwGitHubDeviceAuth(QString clientIdentifier, QObject* parent = nullptr);
    virtual ~cwGitHubDeviceAuth() = default;

    void requestDeviceCode(const QStringList& scopes);
    void startPollingForAccessToken(const DeviceCodeInfo& info);
    void cancel();

signals:
    void deviceCodeReceived(const cwGitHubDeviceAuth::DeviceCodeInfo& info);
    void accessTokenReceived(const cwGitHubDeviceAuth::AccessTokenResult& result);

private slots:
    void pollOnce();

private:
    QByteArray buildFormBody(const QList<QPair<QString, QString>>& items) const;
    QNetworkRequest makeFormRequest(const QUrl& url) const;

    QNetworkAccessManager m_network;
    QString m_clientIdentifier;

    DeviceCodeInfo m_currentDeviceInfo;
    QTimer m_pollTimer;
    bool m_isPolling = false;
};
