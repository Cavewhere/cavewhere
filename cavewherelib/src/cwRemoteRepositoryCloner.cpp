#include "cwRemoteRepositoryCloner.h"

#include "cwGitHostingProvider.h"
#include "cwRecentProjectModel.h"
#include "cwSaveLoad.h"
#include "cwGitHubIntegration.h"

#include "GitFutureWatcher.h"
#include "GitRepository.h"

#include <QDebug>
#include <QDir>
#include <QUrl>

#include <array>

namespace {

constexpr auto kHttp404Token = "404";

// Substrings libgit2 / libcurl produce for DNS, refused, timeout failures.
// Matched case-insensitively against the raw error message.
constexpr std::array<const char*, 4> kUnreachableTokens = {
    "resolve",   // "failed to resolve address" / "could not resolve host"
    "connect",   // "failed to connect to ..." / "connection refused"
    "timed out",
    "timeout"
};

}

cwRemoteRepositoryCloner::cwRemoteRepositoryCloner(QObject* parent) :
    QObject(parent),
    m_cloneRepository(new QQuickGit::GitRepository(this))
{
}

void cwRemoteRepositoryCloner::setRecentProjectModel(cwRecentProjectModel* recentProjectModel)
{
    m_recentProjectModel = recentProjectModel;
}

void cwRemoteRepositoryCloner::setCloneWatcher(QQuickGit::GitFutureWatcher* cloneWatcher)
{
    if (m_cloneWatcher == cloneWatcher) {
        return;
    }

    if (m_cloneWatcher) {
        disconnect(m_cloneWatcher, nullptr, this, nullptr);
    }

    m_cloneWatcher = cloneWatcher;

    if (m_cloneWatcher) {
        connect(m_cloneWatcher, &QQuickGit::GitFutureWatcher::stateChanged,
                this, &cwRemoteRepositoryCloner::handleCloneWatcherStateChanged);
        connect(m_cloneWatcher, &QQuickGit::GitFutureWatcher::progressTextChanged,
                this, &cwRemoteRepositoryCloner::handleCloneWatcherProgressTextChanged);
    }
}

void cwRemoteRepositoryCloner::setGitHubIntegration(cwGitHubIntegration* gh)
{
    if (m_gitHubIntegration == gh) {
        return;
    }
    if (m_gitHubIntegration) {
        disconnect(m_gitHubIntegration, &cwGitHubIntegration::accessTokenChanged,
                   this, nullptr);
    }
    m_gitHubIntegration = gh;
    auto updateCredentials = [this]() {
        const QString token = m_gitHubIntegration
                              ? m_gitHubIntegration->accessToken()
                              : QString{};
        if (shouldAutoRetryClone(m_cloneErrorKind, token, m_pendingCloneUrl)) {
            clone(m_pendingCloneUrl, m_pendingCloneParentDir);
        } else {
            m_cloneRepository->setCredentials(QQuickGit::GitCredentials{token});
        }
    };
    if (gh) {
        connect(gh, &cwGitHubIntegration::accessTokenChanged, this, updateCredentials);
    }
    updateCredentials();
    emit gitHubIntegrationChanged();
}

void cwRemoteRepositoryCloner::setAccount(QQuickGit::Account* account)
{
    if (m_account == account) {
        return;
    }

    m_account = account;
    if (m_cloneRepository) {
        m_cloneRepository->setAccount(m_account);
    }
    emit accountChanged();
}

QString cwRemoteRepositoryCloner::repositoryNameFromUrl(const QString& urlText) const
{
    QString trimmed = urlText.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    while (trimmed.endsWith('/')) {
        trimmed.chop(1);
    }

    int splitIndex = qMax(trimmed.lastIndexOf('/'), trimmed.lastIndexOf(':'));
    QString name = splitIndex >= 0 ? trimmed.mid(splitIndex + 1) : trimmed;
    if (name.endsWith(QStringLiteral(".git"))) {
        name.chop(4);
    }

    return name;
}

QString cwRemoteRepositoryCloner::normalizeCloneUrl(const QString& urlText) const
{
    QString trimmed = urlText.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    if (isSshUrl(trimmed)) {
        int colonIndex = trimmed.lastIndexOf(':');
        return QStringLiteral("ssh://") + trimmed.left(colonIndex) + '/' + trimmed.mid(colonIndex + 1);
    }

    if (!trimmed.contains(QStringLiteral("://"))) {
        return QStringLiteral("https://") + trimmed;
    }

    return trimmed;
}

bool cwRemoteRepositoryCloner::isSshUrl(const QString& urlText) const
{
    const QString trimmed = urlText.trimmed();
    if (trimmed.startsWith(QStringLiteral("ssh://"))) {
        return true;
    }
    if (trimmed.contains(QStringLiteral("://"))) {
        return false;
    }
    const int atIndex = trimmed.indexOf('@');
    const int colonIndex = trimmed.lastIndexOf(':');
    return atIndex != -1 && colonIndex > atIndex;
}

void cwRemoteRepositoryCloner::resetCloneRepository()
{
    delete m_cloneRepository;
    m_cloneRepository = new QQuickGit::GitRepository(this);
    if (m_account) {
        m_cloneRepository->setAccount(m_account);
    }
    if (m_gitHubIntegration) {
        m_cloneRepository->setCredentials(
            QQuickGit::GitCredentials{m_gitHubIntegration->accessToken()});
    }
}

void cwRemoteRepositoryCloner::clone(const QString& urlText)
{
    clone(urlText, QUrl());
}

void cwRemoteRepositoryCloner::clone(const QString& urlText, const QUrl& destinationParentDir)
{
    resetCloneState();

    if (!m_recentProjectModel) {
        qWarning() << "RemoteRepositoryCloner requires recentProjectModel to be set before cloning.";
        return;
    }

    if (!m_cloneWatcher) {
        qWarning() << "RemoteRepositoryCloner requires cloneWatcher to be set before cloning.";
        return;
    }

    QString normalizedUrl = normalizeCloneUrl(urlText);
    if (normalizedUrl.isEmpty()) {
        setCloneErrorMessage(QStringLiteral("Repository URL is empty."));
        return;
    }

    QString repoName = repositoryNameFromUrl(normalizedUrl);
    if (repoName.isEmpty()) {
        setCloneErrorMessage(QStringLiteral("Repository name is missing."));
        return;
    }

    const QUrl cloneParentDir = destinationParentDir.isValid()
        ? destinationParentDir
        : m_recentProjectModel->defaultRepositoryDir();

    cwResultDir resultDir = cwSaveLoad::repositoryDir(cloneParentDir, repoName);
    if (resultDir.hasError()) {
        setCloneErrorMessage(resultDir.errorMessage());
        return;
    }

    const QDir dir = resultDir.value();
    setPendingCloneDir(dir.path());
    m_pendingCloneUrl = normalizedUrl;
    m_pendingCloneParentDir = cloneParentDir;

    resetCloneRepository();
    m_cloneRepository->setDirectory(dir);
    setCloneStatusMessage(QStringLiteral("Starting clone..."));
    m_cloneWatcher->setFuture(m_cloneRepository->clone(QUrl(normalizedUrl)));
}

void cwRemoteRepositoryCloner::resetCloneState()
{
    setCloneErrorMessage(QString());
    setCloneStatusMessage(QString());
    applyCloneError(None);
}

void cwRemoteRepositoryCloner::setCloneErrorMessage(const QString& message)
{
    if (m_cloneErrorMessage == message) {
        return;
    }
    m_cloneErrorMessage = message;
    emit cloneErrorMessageChanged();
}

void cwRemoteRepositoryCloner::setCloneStatusMessage(const QString& message)
{
    if (m_cloneStatusMessage == message) {
        return;
    }
    m_cloneStatusMessage = message;
    emit cloneStatusMessageChanged();
}

void cwRemoteRepositoryCloner::setPendingCloneDir(const QString& dir)
{
    if (m_pendingCloneDir == dir) {
        return;
    }
    m_pendingCloneDir = dir;
    emit pendingCloneDirChanged();
}

void cwRemoteRepositoryCloner::setCloneErrorKind(CloneErrorKind kind)
{
    if (m_cloneErrorKind == kind) {
        return;
    }
    m_cloneErrorKind = kind;
    emit cloneErrorKindChanged();
    emit cloneFailedDueToAuthErrorChanged();
}

void cwRemoteRepositoryCloner::setCloneErrorFriendlyMessage(const QString& message)
{
    if (m_cloneErrorFriendlyMessage == message) {
        return;
    }
    m_cloneErrorFriendlyMessage = message;
    emit cloneErrorFriendlyMessageChanged();
}

void cwRemoteRepositoryCloner::applyCloneError(CloneErrorKind kind)
{
    setCloneErrorFriendlyMessage(friendlyCloneErrorMessage(kind, QUrl(m_pendingCloneUrl)));
    setCloneErrorKind(kind);
}

cwRemoteRepositoryCloner::CloneErrorKind cwRemoteRepositoryCloner::classifyCloneError(
    int errorCode, const QString& errorMessage)
{
    if (errorCode == static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed)) {
        return Auth;
    }
    if (errorMessage.contains(QLatin1String(kHttp404Token))) {
        return NotFoundOrAccess;
    }
    for (const char* token : kUnreachableTokens) {
        if (errorMessage.contains(QLatin1String(token), Qt::CaseInsensitive)) {
            return HostUnreachable;
        }
    }
    return Other;
}

QString cwRemoteRepositoryCloner::friendlyCloneErrorMessage(CloneErrorKind kind, const QUrl& cloneUrl)
{
    const auto& info = cwGitHostingProvider::forUrl(cloneUrl);

    switch (kind) {
    case None:
    case Other:
        return QString();
    case Auth:
        return info.authMessage;
    case NotFoundOrAccess:
        return cwGitHostingProvider::notFoundOrAccessMessage(info, cloneUrl);
    case HostUnreachable: {
        const QString host = cloneUrl.host();
        const QString hostDisplay = host.isEmpty() ? QStringLiteral("the server") : host;
        return QStringLiteral(
            "Couldn't reach %1. Check your internet connection and "
            "that the repository URL is correct.").arg(hostDisplay);
    }
    }
    return QString();
}

bool cwRemoteRepositoryCloner::shouldAutoRetryClone(CloneErrorKind kind,
                                                    const QString& token,
                                                    const QString& pendingUrl)
{
    return kind == Auth && !token.isEmpty() && !pendingUrl.isEmpty();
}

void cwRemoteRepositoryCloner::handleCloneWatcherStateChanged()
{
    if (!m_cloneWatcher) {
        return;
    }

    if (m_cloneWatcher->state() != QQuickGit::GitFutureWatcher::Ready) {
        return;
    }

    if (m_cloneWatcher->hasError()) {
        const auto result = m_cloneWatcher->future().result();
        const QString rawMessage = m_cloneWatcher->errorMessage();
        const CloneErrorKind kind = classifyCloneError(result.errorCode(), rawMessage);

        setCloneErrorMessage(rawMessage);
        applyCloneError(kind);
        setCloneStatusMessage(QString());
        if (!m_pendingCloneDir.isEmpty()) {
            QDir(m_pendingCloneDir).removeRecursively();
        }
        setPendingCloneDir(QString());
        if (kind != Auth) {
            m_pendingCloneUrl.clear();
            m_pendingCloneParentDir = QUrl();
        }
        return;
    }

    if (m_pendingCloneDir.isEmpty() || !m_recentProjectModel) {
        setPendingCloneDir(QString());
        m_pendingCloneUrl.clear();
        return;
    }

    Monad::ResultBase addResult = m_recentProjectModel->addRepositoryDirectory(QDir(m_pendingCloneDir));
    if (addResult.hasError()) {
        setCloneErrorMessage(addResult.errorMessage());
    } else {
        setCloneStatusMessage(QStringLiteral("Clone complete."));
        emit repositoryCloned(m_pendingCloneDir);
        emit repositoryClonedWithRemote(m_pendingCloneDir, m_pendingCloneUrl);
        int clonedIndex = -1;
        const QString cloneDirPath = QDir(m_pendingCloneDir).absolutePath();
        for (int row = 0; row < m_recentProjectModel->rowCount(); ++row) {
            const QString rowPath = m_recentProjectModel->data(m_recentProjectModel->index(row, 0),
                                                            cwRecentProjectModel::PathRole).toString();
            const QFileInfo rowInfo(rowPath);
            const QString rowDirPath = rowInfo.isDir() ? rowInfo.absoluteFilePath()
                                                       : rowInfo.absoluteDir().absolutePath();
            if (rowDirPath == cloneDirPath) {
                clonedIndex = row;
                break;
            }
        }
        emit repositoryClonedIndex(clonedIndex);
    }
    setPendingCloneDir(QString());
    m_pendingCloneUrl.clear();
}

void cwRemoteRepositoryCloner::handleCloneWatcherProgressTextChanged()
{
    if (!m_cloneWatcher) {
        return;
    }

    if (m_cloneWatcher->state() == QQuickGit::GitFutureWatcher::Loading) {
        setCloneStatusMessage(m_cloneWatcher->progressText());
    }
}
