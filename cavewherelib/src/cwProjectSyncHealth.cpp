#include "cwProjectSyncHealth.h"
#include "cwRemoteAuthProvider.h"

#include "GitRemoteInfo.h"
#include "GitRepository.h"

#include "asyncfuture.h"
#include <algorithm>

cwProjectSyncHealth::cwProjectSyncHealth(QObject* parent) :
    QObject(parent),
    m_remoteStatusRestarter(this)
{
    m_pollTimer.setInterval(5 * 60 * 1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &cwProjectSyncHealth::refresh);

    m_remoteStatusRestarter.onFutureChanged([this]() {
        const auto future = m_remoteStatusRestarter.future();
        // The Restarter coalesces a burst of refresh() calls into a single run
        // and fires onFutureChanged once per run (when a fresh future is
        // installed). Tag this run with a generation so a delivery superseded by
        // a newer run — which fires onFutureChanged again — is dropped. The
        // request context (remote/branch/local-changes) is read at delivery
        // time: within a burst the latest scheduleRemoteStatusRefresh wins, and
        // that is exactly the run the coalesced future ends up delivering.
        const int generation = ++m_remoteStatusGeneration;

        AsyncFuture::observe(future)
            .context(this, [this, future, generation]() {
                if (future.isCanceled() || generation != m_remoteStatusGeneration) {
                    return;
                }

                const bool localChanges = m_pendingLocalChanges;
                const bool usesTokenAuth = m_pendingUsesTokenAuth;
                const QString remoteName = m_pendingRemoteName;
                const QString branchName = m_pendingBranchName;

                const auto aheadBehindResult = future.result();
                if (aheadBehindResult.hasError()) {
                    const bool authFailed =
                        aheadBehindResult.errorCode()
                        == static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed);
                    const bool credsLoaded = !m_authProvider || m_authProvider->hasLoadedCredentials();
                    const auto syncBlocker = authFailed
                        ? (credsLoaded ? cwSyncStatus::SyncBlocker::Expired
                                       : cwSyncStatus::SyncBlocker::NeedsLogin)
                        : cwSyncStatus::SyncBlocker::None;
                    setStatus(cwSyncStatus{
                        .m_hasLocalChanges = localChanges,
                        .m_aheadCount = 0,
                        .m_behindCount = 0,
                        .m_stale = true,
                        .m_usesTokenAuth = usesTokenAuth,
                        .m_syncBlocker = syncBlocker,
                        .m_message = authFailed
                            ? (credsLoaded
                               ? QStringLiteral("GitHub access expired.")
                               : QStringLiteral("Sign in to GitHub to check sync status."))
                            : (aheadBehindResult.errorMessage().isEmpty()
                               ? QStringLiteral("Sync status unavailable: upstream branch is missing.")
                               : aheadBehindResult.errorMessage())});
                    return;
                }

                setStatus(cwSyncStatus{
                    .m_hasLocalChanges = localChanges,
                    .m_aheadCount = static_cast<int>(aheadBehindResult.value().ahead),
                    .m_behindCount = static_cast<int>(aheadBehindResult.value().behind),
                    .m_stale = false,
                    .m_usesTokenAuth = usesTokenAuth,
                    .m_message = QStringLiteral("%1/%2: %3 to push, %4 to pull%5")
                                     .arg(remoteName, branchName)
                                     .arg(static_cast<int>(aheadBehindResult.value().ahead))
                                     .arg(static_cast<int>(aheadBehindResult.value().behind))
                                     .arg(localChanges ? QStringLiteral(", local edits pending") : QString())});
            });
    });
}

cwSyncStatus cwProjectSyncHealth::status() const
{
    return m_status;
}

QString cwProjectSyncHealth::remoteName() const
{
    return m_remoteName;
}

void cwProjectSyncHealth::setRemoteName(const QString& name)
{
    if (m_remoteName == name) {
        return;
    }
    m_remoteName = name;
    emit remoteNameChanged();
    refresh();
}

QQuickGit::GitRepository* cwProjectSyncHealth::repository() const
{
    return m_repository;
}

void cwProjectSyncHealth::setRepository(QQuickGit::GitRepository* repository)
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
    emit repositoryChanged();
    if (m_repository) {
        m_modifiedCountConnection = connect(m_repository,
                                            &QQuickGit::GitRepository::modifiedFileCountChanged,
                                            this,
                                            &cwProjectSyncHealth::refresh);
        m_headBranchConnection = connect(m_repository,
                                         &QQuickGit::GitRepository::headBranchNameChanged,
                                         this,
                                         &cwProjectSyncHealth::refresh);
        m_remotesConnection = connect(m_repository,
                                      &QQuickGit::GitRepository::remotesChanged,
                                      this,
                                      &cwProjectSyncHealth::refresh);
        if (!m_pollTimer.isActive()) {
            m_pollTimer.start();
        }
    } else {
        m_pollTimer.stop();
    }

    refresh();
}

void cwProjectSyncHealth::setAuthProvider(cwRemoteAuthProvider* provider)
{
    // Stored only to query hasLoadedCredentials() in onFutureChanged.
    // Signal connections (credentialsLoaded, accessTokenChanged) are managed by cwProject.
    m_authProvider = provider;
}

void cwProjectSyncHealth::refresh()
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
            .m_syncBlocker = cwSyncStatus::SyncBlocker::NoRemote,
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

    QString remoteName;
    if (!m_remoteName.isEmpty()) {
        const bool found = std::any_of(configuredRemotes.cbegin(),
                                       configuredRemotes.cend(),
                                       [this](const QQuickGit::GitRemoteInfo& remoteInfo) {
                                           return remoteInfo.name() == m_remoteName;
                                       });
        if (!found) {
            setStatus(cwSyncStatus{
                .m_hasLocalChanges = localChanges,
                .m_aheadCount = 0,
                .m_behindCount = 0,
                .m_stale = true,
                .m_syncBlocker = cwSyncStatus::SyncBlocker::NoRemote,
                .m_message = QStringLiteral("Remote '%1' is no longer configured.").arg(m_remoteName)});
            return;
        }
        remoteName = m_remoteName;
    } else {
        remoteName = QStringLiteral("origin");
        const bool hasOrigin = std::any_of(configuredRemotes.cbegin(),
                                           configuredRemotes.cend(),
                                           [](const QQuickGit::GitRemoteInfo& remoteInfo) {
                                               return remoteInfo.name() == QStringLiteral("origin");
                                           });
        if (!hasOrigin) {
            remoteName = configuredRemotes.constFirst().name();
        }
    }

    const QUrl remoteUrl = m_repository->remoteUrl(remoteName);
    const bool usesTokenAuth =
        remoteUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;

    scheduleRemoteStatusRefresh(localChanges, remoteName, currentBranch, usesTokenAuth);
}

void cwProjectSyncHealth::scheduleRemoteStatusRefresh(bool hasLocalChanges,
                                                      const QString& remoteName,
                                                      const QString& branchName,
                                                      bool usesTokenAuth)
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
    m_pendingUsesTokenAuth = usesTokenAuth;

    QPointer<QQuickGit::GitRepository> repoPtr = repo;
    m_remoteStatusRestarter.restart([repoPtr, remoteName, branchName]() -> QQuickGit::GitRepository::AheadBehindFuture {
        if (repoPtr == nullptr) {
            return AsyncFuture::completed(Monad::Result<QQuickGit::GitRepository::AheadBehindCounts>(
                QStringLiteral("Sync unavailable: repository is not initialized.")));
        }
        return repoPtr->remoteAheadBehindCommitCounts(remoteName, branchName);
    });
}

void cwProjectSyncHealth::setStatus(cwSyncStatus status)
{
    if (m_status == status) {
        return;
    }

    m_status = std::move(status);
    emit statusChanged();
}
