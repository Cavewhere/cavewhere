#pragma once

#include "CaveWhereLibExport.h"

//Qt
#include <QObject>
#include <QVariantMap>
#include <QRangeModel>
#include <QUrl>
#include <QDeadlineTimer>
#include <QNetworkAccessManager>
#include <QQmlEngine>
#include <QMetaType>
#include <QByteArray>
#include <QPointer>

//Std includes
#include <functional>
#include <memory>
#include <vector>

//Our
#include "cwGitHubDeviceAuth.h"
#include "cwRemoteAuthProvider.h"

class QTimer;
class cwRemoteCredentialStore;

struct CAVEWHERE_LIB_EXPORT cwGitHubRepositoryItem
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

class CAVEWHERE_LIB_EXPORT cwGitHubIntegration : public cwRemoteAuthProvider
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GitHubIntegration)
    QML_UNCREATABLE("Access via RootData.gitHubIntegration")

    friend class cwGitHubIntegrationTestAccess;

    Q_PROPERTY(AuthState authState READ authState NOTIFY authStateChanged)
    Q_PROPERTY(QString userCode READ userCode NOTIFY deviceCodeChanged)
    Q_PROPERTY(QUrl verificationUrl READ verificationUrl NOTIFY deviceCodeChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString lastPollError READ lastPollError NOTIFY lastPollErrorChanged)
    Q_PROPERTY(QRangeModel* repositories READ repositories NOTIFY repositoriesChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int secondsUntilNextPoll READ secondsUntilNextPoll NOTIFY secondsUntilNextPollChanged)
    Q_PROPERTY(bool verificationOpened READ verificationOpened NOTIFY verificationOpenedChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(QString activeAccountId READ activeAccountId NOTIFY activeAccountIdChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY authStateChanged)
    Q_PROPERTY(bool needsInstallation READ needsInstallation NOTIFY needsInstallationChanged)
    Q_PROPERTY(bool installPollActive READ installPollActive NOTIFY installPollActiveChanged)
    Q_PROPERTY(bool installPollTimedOut READ installPollTimedOut NOTIFY installPollTimedOutChanged)
    Q_PROPERTY(QUrl installationUrl READ installationUrl CONSTANT)

public:
    enum class AuthState {
        Idle,
        RequestingCode,
        AwaitingVerification,
        Authorized,
        Error
    };
    Q_ENUM(AuthState)

    explicit cwGitHubIntegration(cwRemoteCredentialStore* credentialStore,
                                 QObject* parent = nullptr);
    ~cwGitHubIntegration() override;

    AuthState authState() const { return m_authState; }
    bool loggedIn() const { return m_authState == AuthState::Authorized; }
    QString userCode() const { return m_deviceInfo.userCode; }
    QUrl verificationUrl() const { return m_deviceInfo.verificationWebAddress; }
    QString errorMessage() const { return m_errorMessage; }
    QString lastPollError() const { return m_lastPollError; }
    QRangeModel* repositories() const { return m_repositories; }
    bool active() const { return m_active; }
    bool busy() const { return m_busy; }
    int secondsUntilNextPoll() const { return m_secondsUntilNextPoll; }
    bool verificationOpened() const { return m_hasOpenedVerificationUrl; }
    QString username() const { return m_username; }
    QString activeAccountId() const { return m_activeAccountId; }
    bool needsInstallation() const { return m_needsInstallation; }
    bool installPollActive() const { return m_installPollActive; }
    bool installPollTimedOut() const { return m_installPollTimedOut; }
    QUrl installationUrl() const;
    QString accessToken() const override { return m_accessToken; }
    bool hasLoadedCredentials() const override { return m_hasLoadedStoredToken; }
    bool tokenLoadedFromKeychain() const { return m_tokenLoadedFromKeychain; }
    void ensureCredentialsLoaded() override;
    // Without a token we can't hit /user/installations; returning true here
    // would stall sync() waiting on a signal that never fires.
    bool supportsInstallationCheck() const override { return !m_accessToken.isEmpty(); }
    void verifyInstallation() override;

    void setActive(bool active);

    Q_INVOKABLE void startDeviceLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void cancelDeviceLoginFlow();
    Q_INVOKABLE void refreshRepositories();
    // Starts polling /user/installations every 3 seconds for up to 3 minutes.
    // Intended to be invoked right after the user opens the GitHub install
    // page in the browser; auto-stops when the app becomes installed (the
    // next poll returns a non-empty installation list) or the deadline passes.
    Q_INVOKABLE void startInstallPolling();
    Q_INVOKABLE void stopInstallPolling();
    Q_INVOKABLE void createRepository(const QString& name,
                                      bool isPrivate,
                                      const QString& org = QString());
    Q_INVOKABLE void reloadAccessTokenFromCredentialStore();
    void clearSession();
    Q_INVOKABLE void markVerificationOpened();
    void logout();
    void setActiveAccountId(const QString& accountId);
    void persistCurrentAccessTokenForAccount(const QString& accountId);
    void invalidateAccountToken(const QString& accountId, const QString& message = QString());
    QByteArray lfsAuthorizationHeader() const;
    void setApiBaseUrl(const QString& url);
    // Test-only: seeds the in-memory access token + Authorized state so unit
    // tests can exercise authenticated request paths without the device flow
    // or a populated credential store. Not for production callers.
    void setAccessTokenForTesting(const QString& token);

signals:
    void authStateChanged();
    void deviceCodeChanged();
    void errorMessageChanged();
    void lastPollErrorChanged();
    void repositoriesChanged();
    void activeChanged();
    void busyChanged();
    void secondsUntilNextPollChanged();
    void verificationOpenedChanged();
    void usernameChanged();
    void activeAccountIdChanged();
    void authorizationSucceeded();
    void authorizationFailed(const QString& message);
    void profileResolved(const QString& username);
    void loggedOut(const QString& accountId);
    void tokenInvalidated(const QString& accountId, const QString& message);
    void repositoryCreated(const cwGitHubRepositoryItem& repo);
    void repositoryCreationFailed(const QString& errorMessage);
    void needsInstallationChanged();
    void installPollActiveChanged();
    void installPollTimedOutChanged();

private:
    struct RepoAggregationState {
        int remaining = 0;
        std::vector<cwGitHubRepositoryItem> aggregated;
        bool finalized = false;
    };

    void setAuthState(AuthState state);
    void setBusy(bool busy);
    void setErrorMessage(const QString& message);
    void setLastPollError(const QString& message);
    void handleDeviceCode(const cwGitHubDeviceAuth::DeviceCodeInfo& info);
    void handleAccessToken(const cwGitHubDeviceAuth::AccessTokenResult& result);
    void handleInstallationsReply(QNetworkReply* reply, bool allowRefreshRetry);
    void fetchInstallationRepositories(const QList<qint64>& installationIds);
    void handleInstallationRepositoriesReply(QNetworkReply* reply,
                                             const std::shared_ptr<RepoAggregationState>& state);
    void handleUserProfileReply(QNetworkReply* reply, bool allowRefreshRetry);
    void handleCreateRepositoryReply(QNetworkReply* reply,
                                     const QString& repoName,
                                     bool isPrivate,
                                     const QString& org,
                                     bool allowRefreshRetry);
    bool isUnauthorizedReply(QNetworkReply* reply) const;
    void invalidateActiveAccountToken(const QString& message);
    QByteArray authorizationHeader() const;
    void storeCredentialsForAccount(const QString& accountId);
    void loadStoredAccessToken();
    void clearStoredCredentials(const QString& accountId);
    void fetchUserProfile(bool allowRefreshRetry = true);
    void refreshRepositoriesInternal(bool allowRefreshRetry);
    void createRepositoryInternal(const QString& name,
                                  bool isPrivate,
                                  const QString& org,
                                  bool allowRefreshRetry);
    void attemptTokenRefresh(std::function<void(bool success)> completion);
    void handleUnauthorized(bool allowRefreshRetry,
                            std::function<void()> onRetry,
                            std::function<void()> onFail);
    bool isAccessTokenNearExpiry() const;
    void setRepositories(std::vector<cwGitHubRepositoryItem> repositories);
    void setNeedsInstallation(bool needsInstallation);
    QString resolveActiveGitHubAccountId() const;

    static QString resolveClientId();
    static QString resolveAppSlug();

private:
    cwGitHubDeviceAuth m_deviceAuth;
    AuthState m_authState = AuthState::Idle;
    QString m_apiBaseUrl = QStringLiteral("https://api.github.com");
    bool m_busy = false;
    cwGitHubDeviceAuth::DeviceCodeInfo m_deviceInfo;
    QString m_accessToken;
    QString m_refreshToken;
    qint64 m_accessTokenExpiresAt = -1; // epoch seconds; -1 means non-expiring / unknown
    QString m_errorMessage;
    QString m_lastPollError;
    QRangeModel* m_repositories = nullptr;
    QNetworkAccessManager m_network;
    bool m_active = false;
    bool m_hasLoadedStoredToken = false;
    bool m_loadingStoredToken = false;
    bool m_tokenLoadedFromKeychain = false;
    QPointer<cwRemoteCredentialStore> m_credentialStore;

    int m_secondsUntilNextPoll = 0;
    bool m_hasOpenedVerificationUrl = false;
    QString m_username;
    QString m_activeAccountId;

    bool m_needsInstallation = false;

    bool m_refreshInFlight = false;
    std::vector<std::function<void(bool)>> m_pendingRefreshCallbacks;

    QTimer* m_installPollTimer = nullptr;
    QDeadlineTimer m_installPollDeadline = QDeadlineTimer::Forever;
    bool m_installPollActive = false;
    bool m_installPollTimedOut = false;
};
