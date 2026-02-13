#pragma once

#include "CaveWhereLibExport.h"
#include "GitRepository.h"
#include "asyncfuture.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

namespace QQuickGit {
class GitRemoteInfo;
}

struct CAVEWHERE_LIB_EXPORT cwSyncStatus
{
    Q_GADGET
    Q_PROPERTY(bool hasLocalChanges READ hasLocalChanges)
    Q_PROPERTY(int aheadCount READ aheadCount)
    Q_PROPERTY(int behindCount READ behindCount)
    Q_PROPERTY(bool stale READ stale)
    Q_PROPERTY(QString message READ message)

public:
    bool hasLocalChanges() const;
    int aheadCount() const;
    int behindCount() const;
    bool stale() const;
    QString message() const;
    bool operator==(const cwSyncStatus& other) const;
    bool operator!=(const cwSyncStatus& other) const;

    bool m_hasLocalChanges = false;
    int m_aheadCount = 0;
    int m_behindCount = 0;
    bool m_stale = true;
    QString m_message;
};
Q_DECLARE_METATYPE(cwSyncStatus)

class CAVEWHERE_LIB_EXPORT cwProjectSyncStatus : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ProjectSyncStatus)

    Q_PROPERTY(bool inProgress READ inProgress NOTIFY inProgressChanged)
    Q_PROPERTY(cwSyncStatus status READ status NOTIFY statusChanged)

public:
    explicit cwProjectSyncStatus(QObject* parent = nullptr);

    bool inProgress() const;
    cwSyncStatus status() const;

    void setRepository(QQuickGit::GitRepository* repository);
    void setInProgress(bool inProgress);

    Q_INVOKABLE void refresh();

signals:
    void inProgressChanged();
    void statusChanged();

private:
    void setStatus(cwSyncStatus status);
    void scheduleRemoteStatusRefresh(bool hasLocalChanges,
                                     const QString& remoteName,
                                     const QString& branchName);

    QPointer<QQuickGit::GitRepository> m_repository;
    QMetaObject::Connection m_modifiedCountConnection;
    QMetaObject::Connection m_headBranchConnection;
    QMetaObject::Connection m_remotesConnection;
    AsyncFuture::Restarter<Monad::Result<QQuickGit::GitRepository::AheadBehindCounts>> m_remoteStatusRestarter;
    QTimer m_pollTimer;

    int m_activeRequestId = 0;
    bool m_pendingLocalChanges = false;
    QString m_pendingRemoteName;
    QString m_pendingBranchName;

    bool m_inProgress = false;
    cwSyncStatus m_status;
};

inline bool cwSyncStatus::hasLocalChanges() const { return m_hasLocalChanges; }
inline int cwSyncStatus::aheadCount() const { return m_aheadCount; }
inline int cwSyncStatus::behindCount() const { return m_behindCount; }
inline bool cwSyncStatus::stale() const { return m_stale; }
inline QString cwSyncStatus::message() const { return m_message; }
inline bool cwSyncStatus::operator==(const cwSyncStatus& other) const
{
    return m_hasLocalChanges == other.m_hasLocalChanges
           && m_aheadCount == other.m_aheadCount
           && m_behindCount == other.m_behindCount
           && m_stale == other.m_stale
           && m_message == other.m_message;
}
inline bool cwSyncStatus::operator!=(const cwSyncStatus& other) const
{
    return !(*this == other);
}
