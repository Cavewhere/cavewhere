#pragma once

//Qt
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QQmlEngine>

//Std includes
#include <memory>

//Our
#include "cwGitHubDeviceAuth.h"

namespace QQuickGit {
class RSAKeyGenerator;
}

class cwGitHubIntegration : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GitHubIntegration)

    Q_PROPERTY(AuthState authState READ authState NOTIFY authStateChanged)
    Q_PROPERTY(QString userCode READ userCode NOTIFY deviceCodeChanged)
    Q_PROPERTY(QUrl verificationUrl READ verificationUrl NOTIFY deviceCodeChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QVariantList repositories READ repositories NOTIFY repositoriesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    enum class AuthState {
        Idle,
        RequestingCode,
        AwaitingVerification,
        Authorized,
        Error
    };
    Q_ENUM(AuthState)

    explicit cwGitHubIntegration(QObject* parent = nullptr);

    AuthState authState() const { return m_authState; }
    QString userCode() const { return m_userCode; }
    QUrl verificationUrl() const { return m_verificationUrl; }
    QString errorMessage() const { return m_errorMessage; }
    QVariantList repositories() const { return m_repositories; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void startDeviceLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void refreshRepositories();
    Q_INVOKABLE QVariantMap ensureKeyPair();
    Q_INVOKABLE void uploadPublicKey(const QString& title);
    Q_INVOKABLE void clearSession();

signals:
    void authStateChanged();
    void deviceCodeChanged();
    void accessTokenChanged();
    void errorMessageChanged();
    void repositoriesChanged();
    void busyChanged();

private:
    void setAuthState(AuthState state);
    void setBusy(bool busy);
    void setErrorMessage(const QString& message);
    void handleDeviceCode(const cwGitHubDeviceAuth::DeviceCodeInfo& info);
    void handleAccessToken(const cwGitHubDeviceAuth::AccessTokenResult& result);
    void handleRepositoryReply(QNetworkReply* reply);
    void handleUploadReply(QNetworkReply* reply);
    QByteArray authorizationHeader() const;
    QString defaultKeyTitle() const;

    static QString resolveClientId();

private:
    cwGitHubDeviceAuth m_deviceAuth;
    AuthState m_authState = AuthState::Idle;
    bool m_busy = false;
    QString m_userCode;
    QUrl m_verificationUrl;
    QString m_accessToken;
    QString m_errorMessage;
    QVariantList m_repositories;
    QNetworkAccessManager m_network;

    std::unique_ptr<QQuickGit::RSAKeyGenerator> m_keyGenerator;
};
