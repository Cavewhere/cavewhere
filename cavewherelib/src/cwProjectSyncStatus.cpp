#include "cwProjectSyncStatus.h"

#include "GitRemoteInfo.h"
#include "GitRepository.h"

#include "asyncfuture.h"
#include <algorithm>

cwProjectSyncStatus::cwProjectSyncStatus(QObject* parent) :
    QObject(parent),
    m_remoteStatusRestarter(this)
{
    m_pollTimer.setInterval(5 * 60 * 1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &cwProjectSyncStatus::refresh);

    m_remoteStatusRestarter.onFutureChanged([this]() {
        const auto future = m_remoteStatusRestarter.future();
        const int requestId = m_activeRequestId;
        const bool localChanges = m_pendingLocalChanges;
        const QString remoteName = m_pendingRemoteName;
        const QString branchName = m_pendingBranchName;

        AsyncFuture::observe(future)
            .context(this, [this, future, requestId, localChanges, remoteName, branchName]() {
                if (future.isCanceled() || requestId != m_activeRequestId) {
                    return;
                }

                const auto aheadBehindResult = future.result();
                if (aheadBehindResult.hasError()) {
                    setStatus(cwSyncStatus{
                        .m_hasLocalChanges = localChanges,
                        .m_aheadCount = 0,
                        .m_behindCount = 0,
                        .m_stale = true,
                        .m_message = aheadBehindResult.errorMessage().isEmpty()
                            ? QStringLiteral("Sync status unavailable: upstream branch is missing.")
                            : aheadBehindResult.errorMessage()});
                    return;
                }

                setStatus(cwSyncStatus{
                    .m_hasLocalChanges = localChanges,
                    .m_aheadCount = static_cast<int>(aheadBehindResult.value().ahead),
                    .m_behindCount = static_cast<int>(aheadBehindResult.value().behind),
                    .m_stale = false,
                    .m_message = QStringLiteral("%1/%2: %3 to push, %4 to pull%5")
                                     .arg(remoteName, branchName)
                                     .arg(static_cast<int>(aheadBehindResult.value().ahead))
                                     .arg(static_cast<int>(aheadBehindResult.value().behind))
                                     .arg(localChanges ? QStringLiteral(", local edits pending") : QString())});
            });
    });
}

bool cwProjectSyncStatus::inProgress() const
{
    return m_inProgress;
}

cwSyncStatus cwProjectSyncStatus::status() const
{
    return m_status;
}

void cwProjectSyncStatus::setRepository(QQuickGit::GitRepository* repository)
{
    if (m_repository == repository) {
        return;
    }

    if (m_modifiedCountConnection) {
        disconnect(m_modifiedCountConnection);
        m_modifiedCountConnection = {};
    }
    if (m_headBranchConnection) {
        disconnect(m_headBranchConnection);
        m_headBranchConnection = {};
    }
    if (m_remotesConnection) {
        disconnect(m_remotesConnection);
        m_remotesConnection = {};
    }

    m_repository = repository;
    if (m_repository) {
        m_modifiedCountConnection = connect(m_repository,
                                            &QQuickGit::GitRepository::modifiedFileCountChanged,
                                            this,
                                            &cwProjectSyncStatus::refresh);
        m_headBranchConnection = connect(m_repository,
                                         &QQuickGit::GitRepository::headBranchNameChanged,
                                         this,
                                         &cwProjectSyncStatus::refresh);
        m_remotesConnection = connect(m_repository,
                                      &QQuickGit::GitRepository::remotesChanged,
                                      this,
                                      &cwProjectSyncStatus::refresh);
        if (!m_pollTimer.isActive()) {
            m_pollTimer.start();
        }
    } else {
        m_pollTimer.stop();
    }

    refresh();
}

void cwProjectSyncStatus::setInProgress(bool inProgress)
{
    if (m_inProgress == inProgress) {
        return;
    }

    m_inProgress = inProgress;
    emit inProgressChanged();
}

void cwProjectSyncStatus::refresh()
{
    if (m_repository == nullptr) {
        m_pollTimer.stop();
        setStatus(cwSyncStatus{
            .m_hasLocalChanges = false,
            .m_aheadCount = 0,
            .m_behindCount = 0,
            .m_stale = true,
            .m_message = QStringLiteral("Sync unavailable: repository is not initialized.")});
        return;
    }

    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
    }

    m_repository->checkStatus();
    const bool localChanges = m_repository->modifiedFileCount() > 0;
    const QVector<QQuickGit::GitRemoteInfo> configuredRemotes = m_repository->remotes();
    if (configuredRemotes.isEmpty()) {
        setStatus(cwSyncStatus{
            .m_hasLocalChanges = localChanges,
            .m_aheadCount = 0,
            .m_behindCount = 0,
            .m_stale = true,
            .m_message = QStringLiteral("No git remote is configured for this project.")});
        return;
    }

    const QString currentBranch = m_repository->headBranchName();
    if (currentBranch.isEmpty()) {
        setStatus(cwSyncStatus{
            .m_hasLocalChanges = localChanges,
            .m_aheadCount = 0,
            .m_behindCount = 0,
            .m_stale = true,
            .m_message = QStringLiteral("Sync status unavailable: no current branch.")});
        return;
    }

    QString remoteName = QStringLiteral("origin");
    const bool hasOrigin = std::any_of(configuredRemotes.cbegin(),
                                       configuredRemotes.cend(),
                                       [](const QQuickGit::GitRemoteInfo& remoteInfo) {
                                           return remoteInfo.name() == QStringLiteral("origin");
                                       });
    if (!hasOrigin) {
        remoteName = configuredRemotes.constFirst().name();
    }

    scheduleRemoteStatusRefresh(localChanges, remoteName, currentBranch);
}

void cwProjectSyncStatus::scheduleRemoteStatusRefresh(bool hasLocalChanges,
                                                      const QString& remoteName,
                                                      const QString& branchName)
{
    auto* repo = m_repository.data();
    if (repo == nullptr) {
        setStatus(cwSyncStatus{
            .m_hasLocalChanges = false,
            .m_aheadCount = 0,
            .m_behindCount = 0,
            .m_stale = true,
            .m_message = QStringLiteral("Sync unavailable: repository is not initialized.")});
        return;
    }

    m_pendingLocalChanges = hasLocalChanges;
    m_pendingRemoteName = remoteName;
    m_pendingBranchName = branchName;
    ++m_activeRequestId;

    QPointer<QQuickGit::GitRepository> repoPtr = repo;
    m_remoteStatusRestarter.restart([repoPtr, remoteName, branchName]() -> QQuickGit::GitRepository::AheadBehindFuture {
        if (repoPtr == nullptr) {
            return AsyncFuture::completed(Monad::Result<QQuickGit::GitRepository::AheadBehindCounts>(
                QStringLiteral("Sync unavailable: repository is not initialized.")));
        }
        return repoPtr->remoteAheadBehindCommitCounts(remoteName, branchName);
    });
}

void cwProjectSyncStatus::setStatus(cwSyncStatus status)
{
    if (m_status == status) {
        return;
    }

    m_status = std::move(status);
    emit statusChanged();
}
