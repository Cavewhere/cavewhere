#pragma once

//Qt
#include <QObject>
#include <QVariantMap>
#include <QRangeModel>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QQmlEngine>
#include <QMetaType>
#include <QByteArray>

//Std includes
#include <memory>
#include <vector>

//Our
#include "cwGitHubDeviceAuth.h"

namespace QQuickGit {
class RSAKeyGenerator;
}

struct cwGitHubRepositoryItem
{
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(QString cloneUrl MEMBER cloneUrl)
    Q_PROPERTY(QString sshUrl MEMBER sshUrl)
    Q_PROPERTY(QString htmlUrl MEMBER htmlUrl)
    Q_PROPERTY(bool isPrivate MEMBER isPrivate)

public:
    QString name;
    QString description;
    QString cloneUrl;
    QString sshUrl;
    QString htmlUrl;
    bool isPrivate = false;
};
Q_DECLARE_METATYPE(cwGitHubRepositoryItem)

QT_BEGIN_NAMESPACE
template<>
struct QRangeModel::RowOptions<::cwGitHubRepositoryItem>
{
    static constexpr auto rowCategory = QRangeModel::RowCategory::MultiRoleItem;
};
QT_END_NAMESPACE

class cwGitHubIntegration : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GitHubIntegration)
    QML_UNCREATABLE("Access via RootData.gitHubIntegration")

    Q_PROPERTY(AuthState authState READ authState NOTIFY authStateChanged)
    Q_PROPERTY(QString userCode READ userCode NOTIFY deviceCodeChanged)
    Q_PROPERTY(QUrl verificationUrl READ verificationUrl NOTIFY deviceCodeChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QRangeModel* repositories READ repositories NOTIFY repositoriesChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int secondsUntilNextPoll READ secondsUntilNextPoll NOTIFY secondsUntilNextPollChanged)
    Q_PROPERTY(bool verificationOpened READ verificationOpened NOTIFY verificationOpenedChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)

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
    QRangeModel* repositories() const { return m_repositories; }
    bool active() const { return m_active; }
    bool busy() const { return m_busy; }
    int secondsUntilNextPoll() const { return m_secondsUntilNextPoll; }
    bool verificationOpened() const { return m_hasOpenedVerificationUrl; }
    QString username() const { return m_username; }

    void setActive(bool active);

    Q_INVOKABLE void startDeviceLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void cancelDeviceLoginFlow();
    Q_INVOKABLE void refreshRepositories();
    Q_INVOKABLE QVariantMap ensureKeyPair();
    Q_INVOKABLE void uploadPublicKey(const QString& title);
    Q_INVOKABLE void clearSession();
    Q_INVOKABLE void markVerificationOpened();
    Q_INVOKABLE void logout();
    QByteArray lfsAuthorizationHeader() const;

signals:
    void authStateChanged();
    void deviceCodeChanged();
    void accessTokenChanged();
    void errorMessageChanged();
    void repositoriesChanged();
    void activeChanged();
    void busyChanged();
    void secondsUntilNextPollChanged();
    void verificationOpenedChanged();
    void usernameChanged();

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
    void setRepositories(std::vector<cwGitHubRepositoryItem> repositories);

    static QString resolveClientId();

private:
    cwGitHubDeviceAuth m_deviceAuth;
    AuthState m_authState = AuthState::Idle;
    bool m_busy = false;
    cwGitHubDeviceAuth::DeviceCodeInfo m_deviceInfo;
    QString m_accessToken;
    QString m_errorMessage;
    QRangeModel* m_repositories = nullptr;
    QNetworkAccessManager m_network;
    bool m_active = false;
    bool m_hasLoadedStoredToken = false;
    bool m_loadingStoredToken = false;

    std::unique_ptr<QQuickGit::RSAKeyGenerator> m_keyGenerator;
    int m_secondsUntilNextPoll = 0;
    bool m_hasOpenedVerificationUrl = false;
    QString m_username;
};
