#include "cwGitHubIntegration.h"

//Qt
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QScopedPointer>
#include <QScopedPointerDeleteLater>
#include <vector>

//Our
#include "cwRemoteAccountModel.h"
#include "cwRemoteCredentialStore.h"

using namespace Qt::StringLiterals;

namespace {
static constexpr auto GitHubDeviceClientIdEnv = "CAVEWHERE_GITHUB_CLIENT_ID";
static constexpr auto DefaultGitHubClientId = "Iv23liChYuxNRf8RNlCX";
static constexpr auto GitHubAppSlugEnv = "CAVEWHERE_GITHUB_APP_SLUG";
static constexpr auto DefaultGitHubAppSlug = "cavewhere";

static bool isSafeIdentifier(const QString& value)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]+$"));
    return pattern.match(value).hasMatch();
}
}

cwGitHubIntegration::cwGitHubIntegration(cwRemoteCredentialStore* credentialStore,
                                         QObject* parent)
    : cwRemoteAuthProvider(parent)
    , m_deviceAuth(resolveClientId(), this)
    , m_credentialStore(credentialStore)
{
    m_network.setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);

    setRepositories({});

    QObject::connect(&m_deviceAuth, &cwGitHubDeviceAuth::deviceCodeReceived,
                     this, &cwGitHubIntegration::handleDeviceCode);

    QObject::connect(&m_deviceAuth, &cwGitHubDeviceAuth::accessTokenReceived,
                     this, &cwGitHubIntegration::handleAccessToken);

    QObject::connect(&m_deviceAuth, &cwGitHubDeviceAuth::secondsUntilNextPollChanged,
                     this, [this](int seconds) {
                         if (m_secondsUntilNextPoll != seconds) {
                             m_secondsUntilNextPoll = seconds;
                             emit secondsUntilNextPollChanged();
                         }
                     });
}

cwGitHubIntegration::~cwGitHubIntegration() = default;

void cwGitHubIntegration::setActive(bool active)
{
    if (m_active == active) {
        return;
    }

    m_active = active;
    emit activeChanged();

    if (m_active && !m_hasLoadedStoredToken && !m_loadingStoredToken) {
        m_loadingStoredToken = true;
        loadStoredAccessToken();
    } else if (m_active && !m_accessToken.isEmpty() && m_authState == AuthState::Authorized) {
        if (isAccessTokenNearExpiry() && !m_refreshToken.isEmpty()) {
            attemptTokenRefresh([this](bool) {
                fetchUserProfile();
                refreshRepositoriesInternal(true);
            });
        } else {
            fetchUserProfile();
            refreshRepositories();
        }
    }
}

void cwGitHubIntegration::startDeviceLogin()
{
    if (!m_active) {
        setActive(true);
    }

    if (m_authState == AuthState::AwaitingVerification) {
        return;
    }

    setErrorMessage({});
    setNeedsInstallation(false);
    if (m_hasOpenedVerificationUrl) {
        m_hasOpenedVerificationUrl = false;
        emit verificationOpenedChanged();
    }
    if (!m_deviceInfo.deviceCode.isEmpty() || !m_deviceInfo.userCode.isEmpty()) {
        m_deviceInfo = {};
        emit deviceCodeChanged();
    }
    if (m_secondsUntilNextPoll != 0) {
        m_secondsUntilNextPoll = 0;
        emit secondsUntilNextPollChanged();
    }
    setAuthState(AuthState::RequestingCode);
    // GitHub Apps use per-installation permissions, not OAuth scopes.
    m_deviceAuth.requestDeviceCode({});
}

void cwGitHubIntegration::cancelLogin()
{
    m_deviceAuth.cancel();
    if (!m_deviceInfo.deviceCode.isEmpty() || !m_deviceInfo.userCode.isEmpty()) {
        m_deviceInfo = {};
        emit deviceCodeChanged();
    }
    m_accessToken.clear();
    m_refreshToken.clear();
    m_accessTokenExpiresAt = -1;
    setNeedsInstallation(false);
    setRepositories({});
    emit accessTokenChanged();
    setAuthState(AuthState::Idle);

    if (m_secondsUntilNextPoll != 0) {
        m_secondsUntilNextPoll = 0;
        emit secondsUntilNextPollChanged();
    }
    if (m_hasOpenedVerificationUrl) {
        m_hasOpenedVerificationUrl = false;
        emit verificationOpenedChanged();
    }
}

void cwGitHubIntegration::cancelDeviceLoginFlow()
{
    m_deviceAuth.cancel();
    if (!m_deviceInfo.deviceCode.isEmpty() || !m_deviceInfo.userCode.isEmpty()) {
        m_deviceInfo = {};
        emit deviceCodeChanged();
    }

    if (m_secondsUntilNextPoll != 0) {
        m_secondsUntilNextPoll = 0;
        emit secondsUntilNextPollChanged();
    }
    if (m_hasOpenedVerificationUrl) {
        m_hasOpenedVerificationUrl = false;
        emit verificationOpenedChanged();
    }

    if (m_accessToken.isEmpty()) {
        setAuthState(AuthState::Idle);
    } else {
        setAuthState(AuthState::Authorized);
    }
}

void cwGitHubIntegration::refreshRepositories()
{
    refreshRepositoriesInternal(true);
}

void cwGitHubIntegration::refreshRepositoriesInternal(bool allowRefreshRetry)
{
    if (!m_active) {
        return;
    }

    if (m_accessToken.isEmpty()) {
        setErrorMessage(tr("Please sign in before listing repositories."));
        return;
    }

    setBusy(true);
    QUrl url(m_apiBaseUrl + QStringLiteral("/user/installations"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QByteArrayLiteral("CaveWhere"));
    request.setRawHeader("Authorization", authorizationHeader());

    QNetworkReply* reply = m_network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, allowRefreshRetry]() {
        handleInstallationsReply(reply, allowRefreshRetry);
    });
}

void cwGitHubIntegration::reloadAccessTokenFromCredentialStore()
{
    if (!m_active) {
        setActive(true);
    }

    const bool hadToken = !m_accessToken.isEmpty();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_accessTokenExpiresAt = -1;
    if (hadToken) {
        emit accessTokenChanged();
    }
    if (!m_username.isEmpty()) {
        m_username.clear();
        emit usernameChanged();
    }

    setNeedsInstallation(false);
    setRepositories({});
    setErrorMessage({});
    setAuthState(AuthState::Idle);
    m_hasLoadedStoredToken = false;
    loadStoredAccessToken();
}

void cwGitHubIntegration::clearSession()
{
    cancelLogin();
}

void cwGitHubIntegration::logout()
{
    const QString accountId = resolveActiveGitHubAccountId();

    cancelLogin();
    m_hasOpenedVerificationUrl = false;
    emit verificationOpenedChanged();
    setAuthState(AuthState::Idle);
    clearStoredCredentials(accountId);
    m_accessToken.clear();
    m_refreshToken.clear();
    m_accessTokenExpiresAt = -1;
    emit accessTokenChanged();
    setRepositories({});
    setErrorMessage({});
    if (!m_username.isEmpty()) {
        m_username.clear();
        emit usernameChanged();
    }
    emit loggedOut(accountId);
}

void cwGitHubIntegration::setActiveAccountId(const QString& accountId)
{
    const QString normalized = accountId.trimmed();
    if (m_activeAccountId == normalized) {
        return;
    }

    m_activeAccountId = normalized;
    emit activeAccountIdChanged();

    // Only load from keychain if the integration is already active (e.g., user switching
    // accounts while GitHub is selected). When inactive (bootstrap/page-load), defer loading
    // to setActive(true) so the keychain is not accessed until the user explicitly selects
    // a GitHub account from the UI.
    if (m_active && !normalized.isEmpty() && !m_loadingStoredToken) {
        m_hasLoadedStoredToken = false;
        m_loadingStoredToken = true;
        loadStoredAccessToken();
    }
}

void cwGitHubIntegration::persistCurrentAccessTokenForAccount(const QString& accountId)
{
    if (m_accessToken.isEmpty()) {
        return;
    }

    const QString normalized = accountId.trimmed();
    if (normalized.isEmpty()) {
        return;
    }

    setActiveAccountId(normalized);
    storeCredentialsForAccount(normalized);
}

void cwGitHubIntegration::invalidateAccountToken(const QString& accountId, const QString& message)
{
    const QString normalized = accountId.trimmed();
    if (!normalized.isEmpty()) {
        setActiveAccountId(normalized);
    }
    invalidateActiveAccountToken(message);
}

void cwGitHubIntegration::setAuthState(AuthState state)
{
    if (m_authState == state) {
        return;
    }
    m_authState = state;
    emit authStateChanged();
}

void cwGitHubIntegration::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void cwGitHubIntegration::setErrorMessage(const QString& message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

void cwGitHubIntegration::handleDeviceCode(const cwGitHubDeviceAuth::DeviceCodeInfo& info)
{
    m_deviceInfo = info;
    emit deviceCodeChanged();

    setAuthState(AuthState::AwaitingVerification);
    if (m_hasOpenedVerificationUrl && !m_deviceInfo.deviceCode.isEmpty()) {
        m_deviceAuth.startPollingForAccessToken(m_deviceInfo);
    }
}

void cwGitHubIntegration::handleAccessToken(const cwGitHubDeviceAuth::AccessTokenResult& result)
{
    if (!result.success) {
        if (result.errorName == QStringLiteral("authorization_pending") ||
            result.errorName == QStringLiteral("slow_down")) {
            // Expected interim errors while waiting for the user.
            return;
        }

        setErrorMessage(result.errorDescription.isEmpty() ? result.errorName : result.errorDescription);
        setAuthState(AuthState::Error);
        emit authorizationFailed(m_errorMessage);
        return;
    }

    m_accessToken = result.accessToken;
    m_refreshToken = result.refreshToken;
    m_accessTokenExpiresAt = (result.expiresInSec > 0)
        ? (QDateTime::currentSecsSinceEpoch() + result.expiresInSec)
        : -1;
    m_tokenLoadedFromKeychain = false;
    emit accessTokenChanged();
    setAuthState(AuthState::Authorized);
    emit authorizationSucceeded();
    setErrorMessage({});
    if (m_active) {
        fetchUserProfile();
        refreshRepositories();
    }

    if (!m_deviceInfo.deviceCode.isEmpty() || !m_deviceInfo.userCode.isEmpty()) {
        m_deviceInfo = {};
        emit deviceCodeChanged();
    }
}

void cwGitHubIntegration::markVerificationOpened()
{
    if (!m_hasOpenedVerificationUrl) {
        m_hasOpenedVerificationUrl = true;
        emit verificationOpenedChanged();
        if (m_authState == AuthState::AwaitingVerification && !m_deviceInfo.deviceCode.isEmpty()) {
            m_deviceAuth.startPollingForAccessToken(m_deviceInfo);
        }
    }
}

void cwGitHubIntegration::storeCredentialsForAccount(const QString& accountId)
{
    if (!m_credentialStore || accountId.trimmed().isEmpty()) {
        return;
    }

    const auto provider = cwRemoteAccountModel::Provider::GitHub;
    m_credentialStore->writeAccessToken(provider, accountId, m_accessToken, this);
    m_credentialStore->writeRefreshToken(provider, accountId, m_refreshToken, this);
    m_credentialStore->writeAccessTokenExpiresAt(provider, accountId, m_accessTokenExpiresAt, this);
}

void cwGitHubIntegration::clearStoredCredentials(const QString& accountId)
{
    if (!m_credentialStore || accountId.isEmpty()) {
        return;
    }

    const auto provider = cwRemoteAccountModel::Provider::GitHub;
    m_credentialStore->deleteAccessToken(provider, accountId, this);
    m_credentialStore->deleteRefreshToken(provider, accountId, this);
    m_credentialStore->deleteAccessTokenExpiresAt(provider, accountId, this);
}

void cwGitHubIntegration::ensureCredentialsLoaded()
{
    if (m_hasLoadedStoredToken || m_loadingStoredToken) {
        return;
    }
    m_loadingStoredToken = true;
    loadStoredAccessToken();
}

void cwGitHubIntegration::loadStoredAccessToken()
{
    const QString accountId = resolveActiveGitHubAccountId();
    if (!m_credentialStore || accountId.isEmpty()) {
        m_loadingStoredToken = false;
        // Do NOT mark m_hasLoadedStoredToken = true here: the account ID is not yet
        // known, so we haven't actually attempted a keychain read.  When
        // setActiveAccountId() later supplies a real ID, it will trigger another load.
        emit credentialsLoaded();
        return;
    }

    struct LoadState {
        int remaining = 3;
        bool accessTokenFound = false;
        QString accessToken;
        QString refreshToken;
        qint64 expiresAt = -1;
    };
    auto state = std::make_shared<LoadState>();

    auto finalize = [this, accountId, state]() {
        if (--state->remaining > 0) {
            return;
        }

        m_loadingStoredToken = false;
        m_hasLoadedStoredToken = true;

        if (accountId == resolveActiveGitHubAccountId()
            && state->accessTokenFound && !state->accessToken.isEmpty()
            && state->refreshToken.isEmpty()) {
            // An access token with no companion refresh token is a stale OAuth-App
            // credential from before the GitHub App migration. GitHub App device
            // flow always issues a refresh token. Clear it and force re-auth.
            clearStoredCredentials(accountId);
            emit credentialsLoaded();
            return;
        }

        if (accountId == resolveActiveGitHubAccountId()
            && state->accessTokenFound && !state->accessToken.isEmpty()) {
            m_accessToken = state->accessToken;
            m_refreshToken = state->refreshToken;
            m_accessTokenExpiresAt = state->expiresAt;
            m_tokenLoadedFromKeychain = true;
            emit accessTokenChanged();
            setAuthState(AuthState::Authorized);
            emit authorizationSucceeded();
            setErrorMessage({});
            m_secondsUntilNextPoll = 0;
            emit secondsUntilNextPollChanged();
            if (m_active) {
                if (isAccessTokenNearExpiry() && !m_refreshToken.isEmpty()) {
                    attemptTokenRefresh([this](bool) {
                        // Regardless of refresh outcome, kick off profile/repos.
                        // If refresh failed, the 401 handlers will invalidate.
                        fetchUserProfile();
                        refreshRepositoriesInternal(true);
                    });
                } else {
                    fetchUserProfile();
                    refreshRepositoriesInternal(true);
                }
            }
        }

        emit credentialsLoaded();
    };

    const auto provider = cwRemoteAccountModel::Provider::GitHub;

    m_credentialStore->readAccessToken(provider, accountId, this,
        [state, finalize](const cwRemoteCredentialStore::ReadResult& result) {
            if (result.success && result.found && !result.value.isEmpty()) {
                state->accessTokenFound = true;
                state->accessToken = result.value;
            }
            finalize();
        });

    m_credentialStore->readRefreshToken(provider, accountId, this,
        [state, finalize](const cwRemoteCredentialStore::ReadResult& result) {
            if (result.success && result.found) {
                state->refreshToken = result.value;
            }
            finalize();
        });

    m_credentialStore->readAccessTokenExpiresAt(provider, accountId, this,
        [state, finalize](bool success, bool found, qint64 epochSeconds) {
            if (success && found) {
                state->expiresAt = epochSeconds;
            }
            finalize();
        });
}

void cwGitHubIntegration::fetchUserProfile(bool allowRefreshRetry)
{
    if (m_accessToken.isEmpty()) {
        return;
    }

    QNetworkRequest request(QUrl(m_apiBaseUrl + QStringLiteral("/user")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QByteArrayLiteral("CaveWhere"));
    request.setRawHeader("Authorization", authorizationHeader());

    QNetworkReply* reply = m_network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, allowRefreshRetry]() {
        handleUserProfileReply(reply, allowRefreshRetry);
    });
}

void cwGitHubIntegration::setRepositories(std::vector<cwGitHubRepositoryItem> repositories)
{
    auto* newModel = new QRangeModel(std::move(repositories), this);
    if (m_repositories != nullptr) {
        m_repositories->deleteLater();
    }
    m_repositories = newModel;
    emit repositoriesChanged();
}

void cwGitHubIntegration::handleUserProfileReply(QNetworkReply* reply, bool allowRefreshRetry)
{
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);
    if (isUnauthorizedReply(reply)) {
        handleUnauthorized(allowRefreshRetry,
            [this]() { fetchUserProfile(false); },
            [this]() { invalidateActiveAccountToken(tr("GitHub session expired. Please sign in again.")); });
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to fetch GitHub profile:" << reply->errorString();
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Failed to parse GitHub profile:" << parseError.errorString();
        return;
    }

    const QString login = doc.object().value(QStringLiteral("login")).toString();
    if (login.isEmpty()) {
        return;
    }

    if (login != m_username) {
        m_username = login;
        emit usernameChanged();
    }
    emit profileResolved(login);
}

void cwGitHubIntegration::handleInstallationsReply(QNetworkReply* reply, bool allowRefreshRetry)
{
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);

    if (isUnauthorizedReply(reply)) {
        handleUnauthorized(allowRefreshRetry,
            [this]() { refreshRepositoriesInternal(false); },
            [this]() {
                setBusy(false);
                invalidateActiveAccountToken(tr("GitHub session expired. Please sign in again."));
            });
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        setBusy(false);
        setErrorMessage(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setBusy(false);
        setErrorMessage(tr("Failed to parse installations list."));
        return;
    }

    const QJsonArray installations = doc.object().value(QStringLiteral("installations")).toArray();

    if (installations.isEmpty()) {
        setBusy(false);
        setRepositories({});
        setNeedsInstallation(true);
        setErrorMessage({});
        return;
    }

    // At least one installation exists; app is installed.
    setNeedsInstallation(false);

    QList<qint64> installationIds;
    installationIds.reserve(installations.size());
    for (const QJsonValue& entry : installations) {
        const qint64 id = static_cast<qint64>(entry.toObject().value(QStringLiteral("id")).toInteger(-1));
        if (id > 0) {
            installationIds.append(id);
        }
    }

    if (installationIds.isEmpty()) {
        // Unexpected shape but non-empty array; treat as "installed, no repos".
        setBusy(false);
        setRepositories({});
        setErrorMessage({});
        return;
    }

    fetchInstallationRepositories(installationIds);
}

void cwGitHubIntegration::fetchInstallationRepositories(const QList<qint64>& installationIds)
{
    auto state = std::make_shared<RepoAggregationState>();
    state->remaining = installationIds.size();

    for (const qint64 installationId : installationIds) {
        QUrl url(m_apiBaseUrl
                 + QStringLiteral("/user/installations/%1/repositories").arg(installationId));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("User-Agent", QByteArrayLiteral("CaveWhere"));
        request.setRawHeader("Authorization", authorizationHeader());

        QNetworkReply* reply = m_network.get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, state]() {
            handleInstallationRepositoriesReply(reply, state);
        });
    }
}

void cwGitHubIntegration::handleInstallationRepositoriesReply(
    QNetworkReply* reply,
    const std::shared_ptr<RepoAggregationState>& state)
{
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);

    if (!state || state->finalized) {
        return;
    }

    // Parse this installation's repos into the aggregated list. Errors on a single
    // installation don't abort the whole listing — they just contribute zero repos.
    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->readAll();
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonArray repos = doc.object().value(QStringLiteral("repositories")).toArray();
            for (const QJsonValue& entry : repos) {
                const QJsonObject obj = entry.toObject();
                cwGitHubRepositoryItem repo;
                repo.name = obj.value(QStringLiteral("name")).toString();
                repo.description = obj.value(QStringLiteral("description")).toString();
                repo.cloneUrl = obj.value(QStringLiteral("clone_url")).toString();
                repo.sshUrl = obj.value(QStringLiteral("ssh_url")).toString();
                repo.htmlUrl = obj.value(QStringLiteral("html_url")).toString();
                repo.isPrivate = obj.value(QStringLiteral("private")).toBool();
                state->aggregated.push_back(std::move(repo));
            }
        }
    }

    if (--state->remaining > 0) {
        return;
    }

    state->finalized = true;
    setBusy(false);
    setRepositories(std::move(state->aggregated));
    setErrorMessage({});
}

QByteArray cwGitHubIntegration::authorizationHeader() const
{
    return QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8();
}

bool cwGitHubIntegration::isUnauthorizedReply(QNetworkReply* reply) const
{
    if (reply == nullptr) {
        return false;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return statusCode == 401;
}

void cwGitHubIntegration::invalidateActiveAccountToken(const QString& message)
{
    const QString accountId = resolveActiveGitHubAccountId();

    if (!accountId.isEmpty()) {
        clearStoredCredentials(accountId);
    }

    const bool hadToken = !m_accessToken.isEmpty();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_accessTokenExpiresAt = -1;
    setNeedsInstallation(false);
    if (hadToken) {
        emit accessTokenChanged();
    }

    if (!m_username.isEmpty()) {
        m_username.clear();
        emit usernameChanged();
    }

    setRepositories({});
    setAuthState(AuthState::Error);
    setErrorMessage(message.isEmpty()
                        ? tr("GitHub session expired. Please sign in again.")
                        : message);
    emit tokenInvalidated(accountId, m_errorMessage);
}

QByteArray cwGitHubIntegration::lfsAuthorizationHeader() const
{
    if (m_accessToken.isEmpty()) {
        return {};
    }

    const QByteArray credentials = QByteArrayLiteral("x-access-token:") + m_accessToken.toUtf8();
    return QByteArrayLiteral("Basic ") + credentials.toBase64();
}

void cwGitHubIntegration::setApiBaseUrl(const QString& url)
{
    m_apiBaseUrl = url;
}

void cwGitHubIntegration::createRepository(const QString& name, bool isPrivate, const QString& org)
{
    createRepositoryInternal(name, isPrivate, org, true);
}

void cwGitHubIntegration::createRepositoryInternal(const QString& name,
                                                    bool isPrivate,
                                                    const QString& org,
                                                    bool allowRefreshRetry)
{
    const QUrl url(m_apiBaseUrl
                   + (org.isEmpty()
                      ? QStringLiteral("/user/repos")
                      : QStringLiteral("/orgs/%1/repos").arg(org)));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QByteArrayLiteral("CaveWhere"));
    request.setRawHeader("Authorization", authorizationHeader());

    const QJsonObject body{
        {QStringLiteral("name"), name},
        {QStringLiteral("private"), isPrivate},
        {QStringLiteral("auto_init"), false}
    };
    QNetworkReply* reply = m_network.post(request, QJsonDocument(body).toJson());
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, name, isPrivate, org, allowRefreshRetry]() {
        handleCreateRepositoryReply(reply, name, isPrivate, org, allowRefreshRetry);
    });
}

void cwGitHubIntegration::handleCreateRepositoryReply(QNetworkReply* reply,
                                                       const QString& repoName,
                                                       bool isPrivate,
                                                       const QString& org,
                                                       bool allowRefreshRetry)
{
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QByteArray data = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    const QJsonObject obj = (parseError.error == QJsonParseError::NoError && doc.isObject())
                                ? doc.object()
                                : QJsonObject{};
    const QString githubMessage = obj.value(QStringLiteral("message")).toString();

    if (statusCode == 401 || statusCode == 403) {
        // 403 is not recoverable via refresh; only 401 triggers a retry attempt.
        const bool canRetry = (statusCode == 401) && allowRefreshRetry;
        handleUnauthorized(canRetry,
            [this, repoName, isPrivate, org]() {
                createRepositoryInternal(repoName, isPrivate, org, false);
            },
            [this, githubMessage]() {
                invalidateActiveAccountToken(githubMessage.isEmpty()
                                                 ? tr("GitHub session expired. Please sign in again.")
                                                 : githubMessage);
                emit repositoryCreationFailed(m_errorMessage);
            });
        return;
    }

    if (statusCode == 422) {
        const QJsonArray errors = obj.value(QStringLiteral("errors")).toArray();
        const QString detail = errors.isEmpty()
                                   ? githubMessage
                                   : errors.first().toObject().value(QStringLiteral("message")).toString();
        emit repositoryCreationFailed(
            tr("Repository name '%1' is already taken or invalid: %2").arg(repoName, detail));
        return;
    }

    if (statusCode < 200 || statusCode >= 300) {
        emit repositoryCreationFailed(
            tr("GitHub returned an error (%1): %2").arg(statusCode).arg(
                githubMessage.isEmpty() ? reply->errorString() : githubMessage));
        return;
    }

    cwGitHubRepositoryItem repo;
    repo.name = obj.value(QStringLiteral("name")).toString();
    repo.cloneUrl = obj.value(QStringLiteral("clone_url")).toString();
    repo.sshUrl = obj.value(QStringLiteral("ssh_url")).toString();
    repo.htmlUrl = obj.value(QStringLiteral("html_url")).toString();
    repo.isPrivate = obj.value(QStringLiteral("private")).toBool();
    emit repositoryCreated(repo);
}

QString cwGitHubIntegration::resolveActiveGitHubAccountId() const
{
    return m_activeAccountId;
}

bool cwGitHubIntegration::isAccessTokenNearExpiry() const
{
    // Token is "near expiry" if we have an expiry timestamp and it's within
    // 60s of the current time (or already past).
    if (m_accessTokenExpiresAt < 0) {
        return false;
    }
    return (m_accessTokenExpiresAt - QDateTime::currentSecsSinceEpoch()) < 60;
}

void cwGitHubIntegration::handleUnauthorized(bool allowRefreshRetry,
                                               std::function<void()> onRetry,
                                               std::function<void()> onFail)
{
    if (allowRefreshRetry && !m_refreshToken.isEmpty()) {
        attemptTokenRefresh([onRetry = std::move(onRetry),
                             onFail = std::move(onFail)](bool ok) {
            if (ok) {
                onRetry();
            } else {
                onFail();
            }
        });
        return;
    }
    onFail();
}

void cwGitHubIntegration::attemptTokenRefresh(std::function<void(bool)> completion)
{
    if (m_refreshToken.isEmpty()) {
        if (completion) {
            completion(false);
        }
        return;
    }

    if (m_refreshInFlight) {
        if (completion) {
            m_pendingRefreshCallbacks.push_back(std::move(completion));
        }
        return;
    }

    m_refreshInFlight = true;

    QNetworkRequest request(QUrl(QStringLiteral("https://github.com/login/oauth/access_token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", QByteArrayLiteral("CaveWhere"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), resolveClientId());
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    const QByteArray body = form.query(QUrl::FullyEncoded).toUtf8();

    QNetworkReply* reply = m_network.post(request, body);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, completion = std::move(completion)]() mutable {
        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);
        const QByteArray data = reply->readAll();
        const auto result = cwGitHubDeviceAuth::parseAccessTokenResponse(data);
        const bool success = result.success && !result.accessToken.isEmpty();

        if (success) {
            m_accessToken = result.accessToken;
            if (!result.refreshToken.isEmpty()) {
                m_refreshToken = result.refreshToken;
            }
            m_accessTokenExpiresAt = (result.expiresInSec > 0)
                ? (QDateTime::currentSecsSinceEpoch() + result.expiresInSec)
                : -1;
            emit accessTokenChanged();

            const QString accountId = resolveActiveGitHubAccountId();
            if (!accountId.isEmpty()) {
                storeCredentialsForAccount(accountId);
            }
        } else if (result.errorName == QStringLiteral("bad_refresh_token")
                   || result.errorName == QStringLiteral("invalid_grant")) {
            // Refresh token is no longer valid on GitHub's side; forget it locally
            // so the next 401 goes straight to the user-prompt path.
            m_refreshToken.clear();
            const QString accountId = resolveActiveGitHubAccountId();
            if (!accountId.isEmpty() && m_credentialStore) {
                m_credentialStore->deleteRefreshToken(cwRemoteAccountModel::Provider::GitHub,
                                                     accountId, this);
            }
        }

        m_refreshInFlight = false;

        auto pending = std::move(m_pendingRefreshCallbacks);
        m_pendingRefreshCallbacks.clear();
        if (completion) {
            completion(success);
        }
        for (auto& cb : pending) {
            if (cb) {
                cb(success);
            }
        }
    });
}

QString cwGitHubIntegration::resolveClientId()
{
    const QByteArray envValue = qgetenv(GitHubDeviceClientIdEnv);
    if (!envValue.isEmpty()) {
        const QString candidate = QString::fromUtf8(envValue);
        if (isSafeIdentifier(candidate)) {
            return candidate;
        }
        qWarning() << "Ignoring" << GitHubDeviceClientIdEnv << "override: value contains characters outside [A-Za-z0-9_-]";
    }
    return QString::fromUtf8(DefaultGitHubClientId);
}

QString cwGitHubIntegration::resolveAppSlug()
{
    const QByteArray envValue = qgetenv(GitHubAppSlugEnv);
    if (!envValue.isEmpty()) {
        const QString candidate = QString::fromUtf8(envValue);
        if (isSafeIdentifier(candidate)) {
            return candidate;
        }
        qWarning() << "Ignoring" << GitHubAppSlugEnv << "override: value contains characters outside [A-Za-z0-9_-]";
    }
    return QString::fromUtf8(DefaultGitHubAppSlug);
}

QUrl cwGitHubIntegration::installationUrl() const
{
    return QUrl(QStringLiteral("https://github.com/apps/%1/installations/new").arg(resolveAppSlug()));
}

void cwGitHubIntegration::setNeedsInstallation(bool needsInstallation)
{
    if (m_needsInstallation == needsInstallation) {
        return;
    }
    m_needsInstallation = needsInstallation;
    emit needsInstallationChanged();
}
