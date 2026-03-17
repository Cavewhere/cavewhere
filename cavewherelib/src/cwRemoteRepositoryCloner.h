#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QPointer>
#include <QString>
#include <QUrl>

#include "cwRecentProjectModel.h"
#include "CaveWhereLibExport.h"
#include "Account.h"
#include "GitFutureWatcher.h"
#include "GitRepository.h"

class CAVEWHERE_LIB_EXPORT cwRemoteRepositoryCloner : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteRepositoryCloner)

    Q_PROPERTY(QString cloneErrorMessage READ cloneErrorMessage NOTIFY cloneErrorMessageChanged)
    Q_PROPERTY(QString cloneStatusMessage READ cloneStatusMessage NOTIFY cloneStatusMessageChanged)
    Q_PROPERTY(QString pendingCloneDir READ pendingCloneDir NOTIFY pendingCloneDirChanged)
    Q_PROPERTY(cwRecentProjectModel* recentProjectModel READ recentProjectModel WRITE setRecentProjectModel REQUIRED)
    Q_PROPERTY(QQuickGit::GitFutureWatcher* cloneWatcher READ cloneWatcher WRITE setCloneWatcher REQUIRED)
    Q_PROPERTY(QQuickGit::Account* account READ account WRITE setAccount NOTIFY accountChanged)

public:
    explicit cwRemoteRepositoryCloner(QObject* parent = nullptr);

    QString cloneErrorMessage() const { return m_cloneErrorMessage; }
    QString cloneStatusMessage() const { return m_cloneStatusMessage; }
    QString pendingCloneDir() const { return m_pendingCloneDir; }

    cwRecentProjectModel* recentProjectModel() const { return m_recentProjectModel; }
    void setRecentProjectModel(cwRecentProjectModel* recentProjectModel);

    QQuickGit::GitFutureWatcher* cloneWatcher() const { return m_cloneWatcher; }
    void setCloneWatcher(QQuickGit::GitFutureWatcher* cloneWatcher);

    QQuickGit::Account* account() const { return m_account; }
    void setAccount(QQuickGit::Account* account);

    Q_INVOKABLE QString repositoryNameFromUrl(const QString& urlText) const;
    Q_INVOKABLE QString normalizeCloneUrl(const QString& urlText) const;
    Q_INVOKABLE void clone(const QString& urlText);
    Q_INVOKABLE void clone(const QString& urlText, const QUrl& destinationParentDir);

signals:
    void cloneErrorMessageChanged();
    void cloneStatusMessageChanged();
    void pendingCloneDirChanged();
    void accountChanged();
    void repositoryCloned(QString repositoryPath);
    void repositoryClonedWithRemote(QString repositoryPath, QString remoteUrl);
    void repositoryClonedIndex(int index);

private:
    void setCloneErrorMessage(const QString& message);
    void setCloneStatusMessage(const QString& message);
    void setPendingCloneDir(const QString& dir);
    void handleCloneWatcherStateChanged();
    void handleCloneWatcherProgressTextChanged();

    QPointer<cwRecentProjectModel> m_recentProjectModel;
    QPointer<QQuickGit::GitFutureWatcher> m_cloneWatcher;
    QPointer<QQuickGit::Account> m_account;
    QQuickGit::GitRepository* m_cloneRepository = nullptr;
    QString m_cloneErrorMessage;
    QString m_cloneStatusMessage;
    QString m_pendingCloneDir;
    QString m_pendingCloneUrl;
};
