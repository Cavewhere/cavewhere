#include "cwGitHubCredentials.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>

#include <limits>

namespace {

constexpr auto KeyVersion = QLatin1StringView("v");
constexpr auto KeyAccessToken = QLatin1StringView("accessToken");
constexpr auto KeyRefreshToken = QLatin1StringView("refreshToken");
constexpr auto KeyExpiresAt = QLatin1StringView("accessTokenExpiresAt");

bool containsControlChar(const QString& value)
{
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if (code < 0x20 || code == 0x7F) {
            return true;
        }
    }
    return false;
}

QString sanitizedTokenString(const QJsonValue& value)
{
    if (!value.isString()) {
        return {};
    }
    const QString str = value.toString();
    if (str.isEmpty() || containsControlChar(str)) {
        return {};
    }
    return str;
}

qint64 parseExpiresAt(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull()) {
        return -1;
    }

    if (value.isDouble()) {
        const double asDouble = value.toDouble();
        constexpr double maxSafe = static_cast<double>(std::numeric_limits<qint64>::max());
        if (asDouble < 0.0 || asDouble > maxSafe) {
            return -1;
        }
        return static_cast<qint64>(asDouble);
    }

    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        if (!ok || parsed < 0) {
            return -1;
        }
        return parsed;
    }

    return -1;
}

}

QJsonObject cwGitHubCredentials::toJson() const
{
    QJsonObject json;
    json.insert(KeyVersion, CurrentVersion);
    json.insert(KeyAccessToken, accessToken);
    json.insert(KeyRefreshToken, refreshToken);
    json.insert(KeyExpiresAt, static_cast<double>(accessTokenExpiresAt));
    return json;
}

cwGitHubCredentials cwGitHubCredentials::fromJson(const QJsonObject& json)
{
    const QJsonValue versionValue = json.value(KeyVersion);
    if (!versionValue.isUndefined()) {
        bool versionOk = false;
        const int parsedVersion = versionValue.toVariant().toInt(&versionOk);
        if (!versionOk || parsedVersion != CurrentVersion) {
            return {};
        }
    }

    cwGitHubCredentials credentials;
    credentials.accessToken = sanitizedTokenString(json.value(KeyAccessToken));
    credentials.refreshToken = sanitizedTokenString(json.value(KeyRefreshToken));
    credentials.accessTokenExpiresAt = parseExpiresAt(json.value(KeyExpiresAt));
    return credentials;
}

QByteArray cwGitHubCredentials::toKeychainBytes() const
{
    return QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
}

cwGitHubCredentials cwGitHubCredentials::fromKeychainBytes(const QByteArray& bytes)
{
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) {
        return {};
    }
    return fromJson(doc.object());
}
