#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QPointer>
#include <QString>

#include "cwRepositoryModel.h"
#include "Account.h"
#include "GitFutureWatcher.h"
#include "GitRepository.h"

class cwRemoteRepositoryCloner : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteRepositoryCloner)

    Q_PROPERTY(QString cloneErrorMessage READ cloneErrorMessage NOTIFY cloneErrorMessageChanged)
    Q_PROPERTY(QString cloneStatusMessage READ cloneStatusMessage NOTIFY cloneStatusMessageChanged)
    Q_PROPERTY(QString pendingCloneDir READ pendingCloneDir NOTIFY pendingCloneDirChanged)
    Q_PROPERTY(cwRepositoryModel* repositoryModel READ repositoryModel WRITE setRepositoryModel REQUIRED)
    Q_PROPERTY(QQuickGit::GitFutureWatcher* cloneWatcher READ cloneWatcher WRITE setCloneWatcher REQUIRED)
    Q_PROPERTY(QQuickGit::Account* account READ account WRITE setAccount NOTIFY accountChanged)

public:
    explicit cwRemoteRepositoryCloner(QObject* parent = nullptr);

    QString cloneErrorMessage() const { return m_cloneErrorMessage; }
    QString cloneStatusMessage() const { return m_cloneStatusMessage; }
    QString pendingCloneDir() const { return m_pendingCloneDir; }

    cwRepositoryModel* repositoryModel() const { return m_repositoryModel; }
    void setRepositoryModel(cwRepositoryModel* repositoryModel);

    QQuickGit::GitFutureWatcher* cloneWatcher() const { return m_cloneWatcher; }
    void setCloneWatcher(QQuickGit::GitFutureWatcher* cloneWatcher);

    QQuickGit::Account* account() const { return m_account; }
    void setAccount(QQuickGit::Account* account);

    Q_INVOKABLE QString repositoryNameFromUrl(const QString& urlText) const;
    Q_INVOKABLE QString normalizeCloneUrl(const QString& urlText) const;
    Q_INVOKABLE void clone(const QString& urlText);

signals:
    void cloneErrorMessageChanged();
    void cloneStatusMessageChanged();
    void pendingCloneDirChanged();
    void accountChanged();

private:
    void setCloneErrorMessage(const QString& message);
    void setCloneStatusMessage(const QString& message);
    void setPendingCloneDir(const QString& dir);
    void handleCloneWatcherStateChanged();
    void handleCloneWatcherProgressTextChanged();

    QPointer<cwRepositoryModel> m_repositoryModel;
    QPointer<QQuickGit::GitFutureWatcher> m_cloneWatcher;
    QPointer<QQuickGit::Account> m_account;
    QQuickGit::GitRepository* m_cloneRepository = nullptr;
    QString m_cloneErrorMessage;
    QString m_cloneStatusMessage;
    QString m_pendingCloneDir;
};
