#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QPointer>
#include <QUrl>

#include "CaveWhereLibExport.h"
#include "cwGitHubIntegration.h"
#include "cwRemoteAccountModel.h"
class cwRemoteBindingStore;
namespace QQuickGit { class GitRepository; }

class CAVEWHERE_LIB_EXPORT cwRemoteAccountCoordinator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteAccountCoordinator)
    QML_UNCREATABLE("Access via RootData.remoteAccountCoordinator")

public:
    explicit cwRemoteAccountCoordinator(cwGitHubIntegration* gitHubIntegration,
                                        cwRemoteAccountModel* remoteAccountModel,
                                        cwRemoteBindingStore* remoteBindingStore,
                                        QObject* parent = nullptr);

    Q_INVOKABLE void forgetGitHubAccount();
    Q_INVOKABLE void forgetGitHubAccount(const QString& username);
    Q_INVOKABLE void startAddGitHubAccount();
    Q_INVOKABLE void selectGitHubAccount(const QString& username);
    Q_INVOKABLE void bindRemoteToActiveGitHubAccount(const QString& remoteUrl);
    Q_INVOKABLE void addRemoteToProject(QQuickGit::GitRepository* repository,
                                        const QString& remoteUrl,
                                        bool bindToGitHubAccount = true);
    void handleGitHubLfsAuthFailure(const QUrl& remoteUrl, int httpStatus, const QString& message);

signals:
    void addRemoteFailed(const QString& errorMessage);

private:
    QString initialGitHubAccountId() const;
    void bootstrapGitHubActiveAccount();
    void syncAuthorizedGitHubAccount();
    void handleGitHubLoggedOut(const QString& accountId);
    void handleGitHubTokenInvalidated(const QString& accountId, const QString& message);
    QString firstAuthorizedGitHubAccountExcluding(const QString& accountId) const;

    QPointer<cwGitHubIntegration> m_gitHubIntegration;
    QPointer<cwRemoteAccountModel> m_remoteAccountModel;
    QPointer<cwRemoteBindingStore> m_remoteBindingStore;
};
