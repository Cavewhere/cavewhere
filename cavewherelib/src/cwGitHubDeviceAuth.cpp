#include "cwGitHubDeviceAuth.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

static QUrl deviceCodeUrl() {
    return QUrl(QStringLiteral("https://github.com/login/device/code"));
}

static QUrl accessTokenUrl() {
    return QUrl(QStringLiteral("https://github.com/login/oauth/access_token"));
}

cwGitHubDeviceAuth::cwGitHubDeviceAuth(QString clientIdentifier, QObject* parent)
    : QObject(parent),
      m_clientIdentifier(std::move(clientIdentifier))
{
    m_pollTimer.setSingleShot(true);
    QObject::connect(&m_pollTimer, &QTimer::timeout, this, &cwGitHubDeviceAuth::poll);

    m_countdownTimer.setInterval(1000);
    QObject::connect(&m_countdownTimer, &QTimer::timeout, this, &cwGitHubDeviceAuth::updateCountdown);
}

QByteArray cwGitHubDeviceAuth::buildFormBody(const QList<QPair<QString, QString>>& items) const {
    QByteArray body;
    bool isFirst = true;
    for (const auto& pair : items) {
        if (!isFirst) {
            body.append('&');
        }
        isFirst = false;
        body.append(QUrl::toPercentEncoding(pair.first));
        body.append('=');
        body.append(QUrl::toPercentEncoding(pair.second));
    }
    return body;
}

QNetworkRequest cwGitHubDeviceAuth::makeFormRequest(const QUrl& url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "YourAppName-Qt");
    return request;
}

void cwGitHubDeviceAuth::requestDeviceCode(const QStringList& scopes) {
    QList<QPair<QString, QString>> params;
    params.append({QStringLiteral("client_id"), m_clientIdentifier});
    if (!scopes.isEmpty()) {
        params.append({QStringLiteral("scope"), scopes.join(' ')});
    }

    QNetworkRequest request = makeFormRequest(deviceCodeUrl());
    QNetworkReply* reply = m_network.post(request, buildFormBody(params));

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            DeviceCodeInfo emptyInfo{};
            emit deviceCodeReceived(emptyInfo);
            return;
        }

        const QJsonObject obj = doc.object();
        DeviceCodeInfo info;
        info.deviceCode = obj.value(QStringLiteral("device_code")).toString();
        info.userCode = obj.value(QStringLiteral("user_code")).toString();
        info.verificationWebAddress = QUrl(obj.value(QStringLiteral("verification_uri")).toString());
        info.expiresInSeconds = obj.value(QStringLiteral("expires_in")).toInt(900);
        info.pollIntervalSeconds = obj.value(QStringLiteral("interval")).toInt(5);

        emit deviceCodeReceived(info);
    });
}

void cwGitHubDeviceAuth::startPollingForAccessToken(const DeviceCodeInfo& info) {
    m_currentDeviceInfo = info;
    m_isPolling = true;
    m_pollRequestInFlight = false;
    m_currentPollIntervalSeconds = qMax(1, m_currentDeviceInfo.pollIntervalSeconds);
    // Respect GitHub pacing by waiting one full interval before first poll.
    scheduleNextPoll(m_currentPollIntervalSeconds);
}

void cwGitHubDeviceAuth::cancel() {
    m_isPolling = false;
    m_pollRequestInFlight = false;
    m_pollTimer.stop();
    resetCountdown(0);
}

void cwGitHubDeviceAuth::poll() {
    if (!m_isPolling) {
        return;
    }
    if (m_pollRequestInFlight) {
        return;
    }

    m_pollRequestInFlight = true;

    QList<QPair<QString, QString>> params;
    params.append({QStringLiteral("client_id"), m_clientIdentifier});
    params.append({QStringLiteral("device_code"), m_currentDeviceInfo.deviceCode});
    params.append({QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code")});

    QNetworkRequest request = makeFormRequest(accessTokenUrl());
    QNetworkReply* reply = m_network.post(request, buildFormBody(params));

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_pollRequestInFlight = false;
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        AccessTokenResult result;

        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            result.success = false;
            result.errorName = QStringLiteral("parse_error");
            result.errorDescription = QStringLiteral("Failed to parse token response.");
            scheduleNextPoll(m_currentPollIntervalSeconds);
            emit accessTokenReceived(result);
            return;
        }

        const QJsonObject obj = doc.object();

        if (obj.contains(QStringLiteral("error"))) {
            result.success = false;

            result.errorName = obj.value(QStringLiteral("error")).toString();
            result.errorDescription = obj.value(QStringLiteral("error_description")).toString();

            if (result.errorName == QStringLiteral("slow_down")) {
                const int serverInterval = obj.value(QStringLiteral("interval")).toInt(0);
                const int intervalSec = serverInterval > 0 ? serverInterval : (m_currentPollIntervalSeconds + 5);
                m_currentPollIntervalSeconds = qMax(1, intervalSec);
                scheduleNextPoll(m_currentPollIntervalSeconds);
            } else if (result.errorName == QStringLiteral("expired_token") ||
                       result.errorName == QStringLiteral("access_denied")) {
                cancel();
            } else if (result.errorName == QStringLiteral("authorization_pending")) {
                scheduleNextPoll(m_currentPollIntervalSeconds);
            } else {
                scheduleNextPoll(m_currentPollIntervalSeconds);
            }
            emit accessTokenReceived(result);
            return;
        }

        result.success = true;
        result.accessToken = obj.value(QStringLiteral("access_token")).toString();
        result.tokenType = obj.value(QStringLiteral("token_type")).toString();
        result.scope = obj.value(QStringLiteral("scope")).toString();

        cancel();
        emit accessTokenReceived(result);
    });
}

void cwGitHubDeviceAuth::updateCountdown()
{
    if (!m_isPolling) {
        resetCountdown(0);
        return;
    }

    if (m_secondsUntilNextPoll > 0) {
        --m_secondsUntilNextPoll;
        emit secondsUntilNextPollChanged(m_secondsUntilNextPoll);
        if (m_secondsUntilNextPoll == 0) {
            m_countdownTimer.stop();
        }
    } else {
        m_countdownTimer.stop();
    }
}

void cwGitHubDeviceAuth::resetCountdown(int seconds)
{
    seconds = qMax(0, seconds);
    if (m_secondsUntilNextPoll != seconds) {
        m_secondsUntilNextPoll = seconds;
        emit secondsUntilNextPollChanged(m_secondsUntilNextPoll);
    }

    if (m_secondsUntilNextPoll > 0) {
        if (!m_countdownTimer.isActive()) {
            m_countdownTimer.start();
        }
    } else {
        m_countdownTimer.stop();
    }
}

void cwGitHubDeviceAuth::scheduleNextPoll(int intervalSeconds)
{
    if (!m_isPolling || m_pollRequestInFlight) {
        return;
    }

    m_currentPollIntervalSeconds = qMax(1, intervalSeconds);
    constexpr int bufferMs = 50;
    constexpr int secToMsec = 1000;
    m_pollTimer.start(m_currentPollIntervalSeconds * secToMsec + bufferMs);
    resetCountdown(m_currentPollIntervalSeconds);
}
