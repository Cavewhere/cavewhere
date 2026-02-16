#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QPointer>

#include "CaveWhereLibExport.h"
#include "cwGitHubIntegration.h"
#include "cwRemoteAccountModel.h"
class cwRemoteBindingStore;

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

private:
    QString initialGitHubAccountId() const;
    void bootstrapGitHubActiveAccount();
    void syncAuthorizedGitHubAccount();
    void handleGitHubLoggedOut(const QString& accountId);

    QPointer<cwGitHubIntegration> m_gitHubIntegration;
    QPointer<cwRemoteAccountModel> m_remoteAccountModel;
    QPointer<cwRemoteBindingStore> m_remoteBindingStore;
};
