/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef LOADPROJECTHELPER
#define LOADPROJECTHELPER

//Qt includes
#include <QVector3D>
#include <QString>
#include <QVariantMap>
#include <QObject>
#include <QTemporaryDir>
#include <QQmlEngine>

//Our includes
#include "cwProject.h"
#include "cwNote.h"
#include "CaveWhereTestLibExport.h"

//Std includes
#include <iostream>

/**
 * Copyies filename to the temp folder
 */
QString CAVEWHERE_TESTLIB_EXPORT copyToTempFolder(QString filename);

QString CAVEWHERE_TESTLIB_EXPORT prependTempFolder(QString filename);

void CAVEWHERE_TESTLIB_EXPORT addTokenManager(cwProject* project);

/**
 * @brief fileToProject
 * @param filename
 * @return A new project generate from filename
 */
std::shared_ptr<cwProject> CAVEWHERE_TESTLIB_EXPORT fileToProject(QString filename);
QString CAVEWHERE_TESTLIB_EXPORT fileToProject(cwProject* project, const QString& filename);

class CAVEWHERE_TESTLIB_EXPORT cwCloneFixtureInfo
{
    Q_GADGET
    QML_NAMED_ELEMENT(cloneFixtureInfo)
    Q_PROPERTY(QString errorMessage MEMBER errorMessage)
    Q_PROPERTY(QString remoteUrl MEMBER remoteUrl)
    Q_PROPERTY(QString remoteRepoPath MEMBER remoteRepoPath)
    Q_PROPERTY(QString cloneParentPath MEMBER cloneParentPath)
    Q_PROPERTY(QString repoName MEMBER repoName)
    Q_PROPERTY(QString expectedClonePath MEMBER expectedClonePath)

public:
    QString errorMessage;
    QString remoteUrl;
    QString remoteRepoPath;
    QString cloneParentPath;
    QString repoName;
    QString expectedClonePath;
};
Q_DECLARE_METATYPE(cwCloneFixtureInfo)

class CAVEWHERE_TESTLIB_EXPORT cwSyncFixtureInfo
{
    Q_GADGET
    QML_NAMED_ELEMENT(syncFixtureInfo)
    Q_PROPERTY(QString errorMessage MEMBER errorMessage)
    Q_PROPERTY(QString projectFilePath MEMBER projectFilePath)
    Q_PROPERTY(QString workingRepoPath MEMBER workingRepoPath)
    Q_PROPERTY(QString remoteRepoPath MEMBER remoteRepoPath)
    Q_PROPERTY(QString branchName MEMBER branchName)
    Q_PROPERTY(QString lfsEndpoint MEMBER lfsEndpoint)

public:
    QString errorMessage;
    QString projectFilePath;
    QString workingRepoPath;
    QString remoteRepoPath;
    QString branchName;
    QString lfsEndpoint;
};
Q_DECLARE_METATYPE(cwSyncFixtureInfo)

class CAVEWHERE_TESTLIB_EXPORT TestHelper : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    Q_INVOKABLE void loadProjectFromFile(cwProject* project, const QString& filename) {
        fileToProject(project, filename);
    }
    Q_INVOKABLE void loadProjectFromPath(cwProject* project, const QString& localPath);

    Q_INVOKABLE void loadProjectFromZip(cwProject* project, const QString& filename);

    Q_INVOKABLE QString copyToTempDir(const QString& filename);

    Q_INVOKABLE bool fileExists(const QUrl& filename) const;
    Q_INVOKABLE bool directoryExists(const QUrl& directory) const;
    Q_INVOKABLE size_t fileSize(const QUrl& filename) const;
    Q_INVOKABLE void removeFile(const QUrl& filename) const;
    Q_INVOKABLE QString environmentVariable(const QString& name) const;
    Q_INVOKABLE QString repositoryRemoteUrl(const QUrl& repositoryDirectory,
                                            const QString& remoteName = QStringLiteral("origin")) const;
    Q_INVOKABLE QString normalizeFileGitUrl(const QString& url) const;
    Q_INVOKABLE QString projectHeadCommitOid(cwProject* project) const;
    Q_INVOKABLE int projectModifiedFileCount(cwProject* project) const;
    Q_INVOKABLE void waitForProjectSaveToFinish(cwProject* project) const;
    Q_INVOKABLE QString checkoutProjectRef(cwProject* project,
                                           const QString& refSpec,
                                           bool force = true) const;
    Q_INVOKABLE int noteScrapCount(cwNote* note) const;
    Q_INVOKABLE QVariantMap scrapOutlineState(cwNote* note, int scrapIndex) const;
    Q_INVOKABLE bool addScrapStation(cwNote* note,
                                     int scrapIndex,
                                     const QString& name,
                                     const QPointF& positionOnNote) const;
    Q_INVOKABLE cwSyncFixtureInfo createLocalSyncFixtureWithLfsServer();
    Q_INVOKABLE cwCloneFixtureInfo createLocalBareRemoteForCloneTest();
    Q_INVOKABLE bool setGitHubAccessTokenForAccount(const QString& accountId,
                                                    const QString& token) const;
    Q_INVOKABLE bool clearGitHubAccessTokenForAccount(const QString& accountId) const;

    Q_INVOKABLE QUrl tempDirectoryUrl() {
        QTemporaryDir dir;
        dir.setAutoRemove(false);
        return QUrl::fromLocalFile(dir.path());
    }

    Q_INVOKABLE QUrl toLocalUrl(const QString& path) {
        return QUrl::fromLocalFile(path);
    }

};

#endif // LOADPROJECTHELPER
