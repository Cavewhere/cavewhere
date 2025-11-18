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
    Q_PROPERTY(int secondsUntilNextPoll READ secondsUntilNextPoll NOTIFY secondsUntilNextPollChanged)
    Q_PROPERTY(bool verificationOpened READ verificationOpened NOTIFY verificationOpenedChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(bool autoLoadStoredAccount READ autoLoadStoredAccount WRITE setAutoLoadStoredAccount NOTIFY autoLoadStoredAccountChanged)

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
    QString userCode() const { return m_deviceInfo.userCode; }
    QUrl verificationUrl() const { return m_deviceInfo.verificationWebAddress; }
    QString errorMessage() const { return m_errorMessage; }
    QVariantList repositories() const { return m_repositories; }
    bool busy() const { return m_busy; }
    int secondsUntilNextPoll() const { return m_secondsUntilNextPoll; }
    bool verificationOpened() const { return m_hasOpenedVerificationUrl; }
    QString username() const { return m_username; }
    bool autoLoadStoredAccount() const { return m_autoLoadStoredAccount; }
    void setAutoLoadStoredAccount(bool enabled);

    Q_INVOKABLE void startDeviceLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void refreshRepositories();
    Q_INVOKABLE QVariantMap ensureKeyPair();
    Q_INVOKABLE void uploadPublicKey(const QString& title);
    Q_INVOKABLE void clearSession();
    Q_INVOKABLE void markVerificationOpened();
    Q_INVOKABLE void logout();

signals:
    void authStateChanged();
    void deviceCodeChanged();
    void accessTokenChanged();
    void errorMessageChanged();
    void repositoriesChanged();
    void busyChanged();
    void secondsUntilNextPollChanged();
    void verificationOpenedChanged();
    void usernameChanged();
    void autoLoadStoredAccountChanged();

private:
    void setAuthState(AuthState state);
    void setBusy(bool busy);
    void setErrorMessage(const QString& message);
    void handleDeviceCode(const cwGitHubDeviceAuth::DeviceCodeInfo& info);
    void handleAccessToken(const cwGitHubDeviceAuth::AccessTokenResult& result);
    void handleRepositoryReply(QNetworkReply* reply);
    void handleUploadReply(QNetworkReply* reply);
    void handleUserProfileReply(QNetworkReply* reply);
    QByteArray authorizationHeader() const;
    QString defaultKeyTitle() const;
    void storeAccessToken(const QString& token);
    void loadStoredAccessToken();
    void clearStoredAccessToken();
    void fetchUserProfile();
    void maybeLoadStoredAccessToken();

    static QString resolveClientId();

private:
    cwGitHubDeviceAuth m_deviceAuth;
    AuthState m_authState = AuthState::Idle;
    bool m_busy = false;
    cwGitHubDeviceAuth::DeviceCodeInfo m_deviceInfo;
    QString m_accessToken;
    QString m_errorMessage;
    QVariantList m_repositories;
    QNetworkAccessManager m_network;

    std::unique_ptr<QQuickGit::RSAKeyGenerator> m_keyGenerator;
    int m_secondsUntilNextPoll = 0;
    bool m_hasOpenedVerificationUrl = false;
    QString m_username;
    bool m_autoLoadStoredAccount = false;
    bool m_loadedStoredAccessToken = false;
};
