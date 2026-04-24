#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include "CaveWhereLibExport.h"

struct CAVEWHERE_LIB_EXPORT cwGitHubCredentials
{
    static constexpr int CurrentVersion = 1;

    QString accessToken;
    QString refreshToken;
    qint64 accessTokenExpiresAt = -1;

    QJsonObject toJson() const;
    static cwGitHubCredentials fromJson(const QJsonObject& json);

    QByteArray toKeychainBytes() const;
    static cwGitHubCredentials fromKeychainBytes(const QByteArray& bytes);
};
