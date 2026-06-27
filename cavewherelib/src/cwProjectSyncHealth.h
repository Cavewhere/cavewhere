#pragma once

#include "CaveWhereLibExport.h"
#include "GitRepository.h"
#include "asyncfuture.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

class cwRemoteAuthProvider;

namespace QQuickGit {
class GitRemoteInfo;
}

struct CAVEWHERE_LIB_EXPORT cwSyncStatus
{
    Q_GADGET
    QML_VALUE_TYPE(cwsyncstatus)
    Q_PROPERTY(bool hasLocalChanges READ hasLocalChanges)
    Q_PROPERTY(int aheadCount READ aheadCount)
    Q_PROPERTY(int behindCount READ behindCount)
    Q_PROPERTY(bool stale READ stale)
    Q_PROPERTY(bool hasRemote READ hasRemote)
    Q_PROPERTY(bool authExpired READ authExpired)
    Q_PROPERTY(bool needsLogin READ needsLogin)
    Q_PROPERTY(bool usesTokenAuth READ usesTokenAuth)
    Q_PROPERTY(QString message READ message)
    Q_PROPERTY(SyncBlocker syncBlocker READ syncBlocker)

public:
    enum class SyncBlocker {
        None,       // sync is unblocked
        NoRemote,   // no remote configured
        NeedsLogin, // HttpAuthFailed; credentials have never been loaded
        Expired     // HttpAuthFailed; credentials were loaded but rejected
    };
    Q_ENUM(SyncBlocker)

    bool hasLocalChanges() const;
    int aheadCount() const;
    int behindCount() const;
    bool stale() const;
    bool hasRemote() const;
    bool authExpired() const;
    bool needsLogin() const;
    bool usesTokenAuth() const;
    QString message() const;
    SyncBlocker syncBlocker() const;
    bool operator==(const cwSyncStatus& other) const;
    bool operator!=(const cwSyncStatus& other) const;

    bool m_hasLocalChanges = false;
    int m_aheadCount = 0;
    int m_behindCount = 0;
    bool m_stale = true;
    bool m_usesTokenAuth = false;
    SyncBlocker m_syncBlocker = SyncBlocker::None;
    QString m_message;
};
Q_DECLARE_METATYPE(cwSyncStatus)

class CAVEWHERE_LIB_EXPORT cwProjectSyncHealth : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ProjectSyncHealth)

    Q_PROPERTY(cwSyncStatus status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString remoteName READ remoteName WRITE setRemoteName NOTIFY remoteNameChanged)
    Q_PROPERTY(QQuickGit::GitRepository* repository READ repository WRITE setRepository NOTIFY repositoryChanged)

public:
    explicit cwProjectSyncHealth(QObject* parent = nullptr);

    cwSyncStatus status() const;

    QString remoteName() const;
    void setRemoteName(const QString& name);

    QQuickGit::GitRepository* repository() const;
    void setRepository(QQuickGit::GitRepository* repository);

    void setAuthProvider(cwRemoteAuthProvider* provider);

    Q_INVOKABLE void refresh();

signals:
    void statusChanged();
    void remoteNameChanged();
    void repositoryChanged();

private:
    void setStatus(cwSyncStatus status);
    void scheduleRemoteStatusRefresh(bool hasLocalChanges,
                                     const QString& remoteName,
                                     const QString& branchName,
                                     bool usesTokenAuth);

    QPointer<QQuickGit::GitRepository> m_repository;
    QPointer<cwRemoteAuthProvider> m_authProvider;
    QMetaObject::Connection m_modifiedCountConnection;
    QMetaObject::Connection m_headBranchConnection;
    QMetaObject::Connection m_remotesConnection;
    AsyncFuture::Restarter<Monad::Result<QQuickGit::GitRepository::AheadBehindCounts>> m_remoteStatusRestarter;
    QTimer m_pollTimer;

    int m_remoteStatusGeneration = 0;
    bool m_pendingLocalChanges = false;
    bool m_pendingUsesTokenAuth = false;
    QString m_pendingRemoteName;
    QString m_pendingBranchName;

    QString m_remoteName;
    cwSyncStatus m_status;
};

inline bool cwSyncStatus::hasLocalChanges() const { return m_hasLocalChanges; }
inline int cwSyncStatus::aheadCount() const { return m_aheadCount; }
inline int cwSyncStatus::behindCount() const { return m_behindCount; }
inline bool cwSyncStatus::stale() const { return m_stale; }
inline bool cwSyncStatus::hasRemote() const { return m_syncBlocker != SyncBlocker::NoRemote; }
inline bool cwSyncStatus::authExpired() const { return m_syncBlocker == SyncBlocker::Expired; }
inline bool cwSyncStatus::needsLogin() const { return m_syncBlocker == SyncBlocker::NeedsLogin; }
inline bool cwSyncStatus::usesTokenAuth() const { return m_usesTokenAuth; }
inline QString cwSyncStatus::message() const { return m_message; }
inline cwSyncStatus::SyncBlocker cwSyncStatus::syncBlocker() const { return m_syncBlocker; }
inline bool cwSyncStatus::operator==(const cwSyncStatus& other) const
{
    return m_hasLocalChanges == other.m_hasLocalChanges
           && m_aheadCount == other.m_aheadCount
           && m_behindCount == other.m_behindCount
           && m_stale == other.m_stale
           && m_usesTokenAuth == other.m_usesTokenAuth
           && m_syncBlocker == other.m_syncBlocker
           && m_message == other.m_message;
}
inline bool cwSyncStatus::operator!=(const cwSyncStatus& other) const
{
    return !(*this == other);
}
