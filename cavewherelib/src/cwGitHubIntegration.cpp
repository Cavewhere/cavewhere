#include "cwGitHubIntegration.h"

//Qt
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QScopedPointer>
#include <QScopedPointerDeleteLater>

//Our
#include "RSAKeyGenerator.h"

using namespace Qt::StringLiterals;

namespace {
static constexpr auto GitHubDeviceClientIdEnv = "CAVEWHERE_GITHUB_CLIENT_ID";
static constexpr auto DefaultGitHubClientId = "Ov23ctOCCgOdD9y2mSs9";
static const QString GitHubApiBase = QStringLiteral("https://api.github.com");
}

cwGitHubIntegration::cwGitHubIntegration(QObject* parent)
    : QObject(parent)
    , m_deviceAuth(resolveClientId(), this)
{
    QObject::connect(&m_deviceAuth, &cwGitHubDeviceAuth::deviceCodeReceived,
                     this, &cwGitHubIntegration::handleDeviceCode);

    QObject::connect(&m_deviceAuth, &cwGitHubDeviceAuth::accessTokenReceived,
                     this, &cwGitHubIntegration::handleAccessToken);
}

void cwGitHubIntegration::startDeviceLogin()
{
    if (m_authState == AuthState::AwaitingVerification) {
        return;
    }

    qDebug() << "Start device login!";

    setErrorMessage({});
    setAuthState(AuthState::RequestingCode);
    m_deviceAuth.requestDeviceCode({QStringLiteral("repo"), QStringLiteral("read:user")});
}

void cwGitHubIntegration::cancelLogin()
{
    m_deviceAuth.cancel();
    m_userCode.clear();
    m_verificationUrl = QUrl();
    m_accessToken.clear();
    m_repositories.clear();
    emit deviceCodeChanged();
    emit accessTokenChanged();
    emit repositoriesChanged();
    setAuthState(AuthState::Idle);
}

void cwGitHubIntegration::refreshRepositories()
{
    if (m_accessToken.isEmpty()) {
        setErrorMessage(tr("Please sign in before listing repositories."));
        return;
    }

    setBusy(true);
    QUrl url(GitHubApiBase + QStringLiteral("/user/repos"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QByteArrayLiteral("CaveWhere"));
    request.setRawHeader("Authorization", authorizationHeader());

    QNetworkReply* reply = m_network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleRepositoryReply(reply);
    });
}

QVariantMap cwGitHubIntegration::ensureKeyPair()
{
    if (!m_keyGenerator) {
        m_keyGenerator = std::make_unique<QQuickGit::RSAKeyGenerator>();
    }
    m_keyGenerator->loadOrGenerate();

    QVariantMap map;
    map.insert(QStringLiteral("publicKeyPath"), m_keyGenerator->publicKeyPath());
    map.insert(QStringLiteral("privateKeyPath"), m_keyGenerator->privateKeyPath());
    map.insert(QStringLiteral("publicKey"), QString::fromUtf8(m_keyGenerator->publicKey()).trimmed());
    return map;
}

void cwGitHubIntegration::uploadPublicKey(const QString& title)
{
    if (m_accessToken.isEmpty()) {
        setErrorMessage(tr("Please sign in before uploading a key."));
        return;
    }

    const auto keyInfo = ensureKeyPair();
    const QString keyValue = keyInfo.value(QStringLiteral("publicKey")).toString();
    if (keyValue.isEmpty()) {
        setErrorMessage(tr("Could not read SSH public key."));
        return;
    }

    setBusy(true);
    QNetworkRequest request(QUrl(GitHubApiBase + QStringLiteral("/user/keys")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QByteArrayLiteral("CaveWhere"));
    request.setRawHeader("Authorization", authorizationHeader());

    QJsonObject payload;
    payload.insert(QStringLiteral("title"), title.isEmpty() ? defaultKeyTitle() : title);
    payload.insert(QStringLiteral("key"), keyValue);

    QNetworkReply* reply = m_network.post(request, QJsonDocument(payload).toJson());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUploadReply(reply);
    });
}

void cwGitHubIntegration::clearSession()
{
    cancelLogin();
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
    m_userCode = info.userCode;
    m_verificationUrl = info.verificationWebAddress;
    emit deviceCodeChanged();

    setAuthState(AuthState::AwaitingVerification);
    m_deviceAuth.startPollingForAccessToken(info);
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
        return;
    }

    qDebug() << "Logged in!";

    m_accessToken = result.accessToken;
    emit accessTokenChanged();
    setAuthState(AuthState::Authorized);
    setErrorMessage({});
    refreshRepositories();
}

void cwGitHubIntegration::handleRepositoryReply(QNetworkReply* reply)
{
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);
    setBusy(false);

    if (reply->error() != QNetworkReply::NoError) {
        setErrorMessage(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        setErrorMessage(tr("Failed to parse repository list."));
        return;
    }

    QVariantList items;
    const auto array = doc.array();
    items.reserve(array.size());
    for (const QJsonValue& entry : array) {
        const QJsonObject obj = entry.toObject();
        QVariantMap repo;
        repo.insert(QStringLiteral("name"), obj.value(QStringLiteral("name")).toString());
        repo.insert(QStringLiteral("description"), obj.value(QStringLiteral("description")).toString());
        repo.insert(QStringLiteral("cloneUrl"), obj.value(QStringLiteral("clone_url")).toString());
        repo.insert(QStringLiteral("sshUrl"), obj.value(QStringLiteral("ssh_url")).toString());
        repo.insert(QStringLiteral("htmlUrl"), obj.value(QStringLiteral("html_url")).toString());
        repo.insert(QStringLiteral("isPrivate"), obj.value(QStringLiteral("private")).toBool());
        items.append(repo);
    }

    m_repositories = items;
    setErrorMessage({});
    emit repositoriesChanged();
}

void cwGitHubIntegration::handleUploadReply(QNetworkReply* reply)
{
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);
    setBusy(false);

    if (reply->error() == QNetworkReply::NoError) {
        setErrorMessage({});
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const auto message = doc.object().value(QStringLiteral("message")).toString();
        if (!message.isEmpty()) {
            setErrorMessage(message);
            return;
        }
    }

    setErrorMessage(reply->errorString());
}

QByteArray cwGitHubIntegration::authorizationHeader() const
{
    return QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8();
}

QString cwGitHubIntegration::defaultKeyTitle() const
{
    QString username = qEnvironmentVariable("USER");
    if (username.isEmpty()) {
        username = QCoreApplication::applicationName();
    }
    return QStringLiteral("CaveWhere (%1)").arg(username);
}

QString cwGitHubIntegration::resolveClientId()
{
    const QByteArray envValue = qgetenv(GitHubDeviceClientIdEnv);
    if (!envValue.isEmpty()) {
        return QString::fromUtf8(envValue);
    }
    return QString::fromUtf8(DefaultGitHubClientId);
}
