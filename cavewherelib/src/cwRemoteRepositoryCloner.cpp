#include "cwRemoteRepositoryCloner.h"

#include "cwRepositoryModel.h"

#include "GitFutureWatcher.h"
#include "GitRepository.h"

#include <QDebug>
#include <QDir>
#include <QUrl>

cwRemoteRepositoryCloner::cwRemoteRepositoryCloner(QObject* parent) :
    QObject(parent),
    m_cloneRepository(new QQuickGit::GitRepository(this))
{
}

void cwRemoteRepositoryCloner::setRepositoryModel(cwRepositoryModel* repositoryModel)
{
    m_repositoryModel = repositoryModel;
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

    if (!trimmed.contains(QStringLiteral("://"))) {
        int atIndex = trimmed.indexOf('@');
        int colonIndex = trimmed.lastIndexOf(':');
        if (atIndex != -1 && colonIndex > atIndex) {
            return QStringLiteral("ssh://") + trimmed.left(colonIndex) + '/' + trimmed.mid(colonIndex + 1);
        }
        return QStringLiteral("https://") + trimmed;
    }

    return trimmed;
}

void cwRemoteRepositoryCloner::clone(const QString& urlText)
{
    setCloneErrorMessage(QString());
    setCloneStatusMessage(QString());

    if (!m_repositoryModel) {
        qWarning() << "RemoteRepositoryCloner requires repositoryModel to be set before cloning.";
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

    cwResultDir resultDir = m_repositoryModel->repositoryDir(m_repositoryModel->defaultRepositoryDir(), repoName);
    if (resultDir.hasError()) {
        setCloneErrorMessage(resultDir.errorMessage());
        return;
    }

    const QDir dir = resultDir.value();
    setPendingCloneDir(dir.path());
    if (m_cloneRepository) {
        m_cloneRepository->setDirectory(dir);
        setCloneStatusMessage(QStringLiteral("Starting clone..."));
        m_cloneWatcher->setFuture(m_cloneRepository->clone(QUrl(normalizedUrl)));
    }
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

void cwRemoteRepositoryCloner::handleCloneWatcherStateChanged()
{
    if (!m_cloneWatcher) {
        return;
    }

    if (m_cloneWatcher->state() != QQuickGit::GitFutureWatcher::Ready) {
        return;
    }

    if (m_cloneWatcher->hasError()) {
        setCloneErrorMessage(m_cloneWatcher->errorMessage());
        setCloneStatusMessage(QString());
        setPendingCloneDir(QString());
        return;
    }

    if (m_pendingCloneDir.isEmpty() || !m_repositoryModel) {
        setPendingCloneDir(QString());
        return;
    }

    Monad::ResultBase addResult = m_repositoryModel->addRepositoryDirectory(QDir(m_pendingCloneDir));
    if (addResult.hasError()) {
        setCloneErrorMessage(addResult.errorMessage());
    } else {
        setCloneStatusMessage(QStringLiteral("Clone complete."));
    }
    setPendingCloneDir(QString());
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
