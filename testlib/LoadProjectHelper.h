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
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QThread>

//Std includes
#include <memory>
#include <optional>
#include <utility>

//Our includes
#include "cwProject.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwNoteLiDAR.h"
#include "cwPDFConverter.h"
#include "CaveWhereTestLibExport.h"
#include "cwFutureManagerModel.h"
#include "cwRootData.h"
#include "cwErrorListModel.h"

namespace QQuickGit {
class GitRepository;
}

/**
 * Returns the shared temp root directory for this test process.
 * Created once per process, removed recursively at exit.
 */
QString CAVEWHERE_TESTLIB_EXPORT sharedTempRoot();

/**
 * Creates a unique subdirectory under sharedTempRoot() and returns its path.
 */
QString CAVEWHERE_TESTLIB_EXPORT createTempSubdir();

inline QString testcasesDatasetSourcePath(const QString& relativePath) {
    return QStringLiteral(CW_TESTCASES_DATASET_DIR "/") + relativePath;
}
inline QString qmlTestDatasetSourcePath(const QString& relativePath) {
    return QStringLiteral(CW_QML_TEST_DATASET_DIR "/") + relativePath;
}

/**
 * Copyies filename to the temp folder
 */
QString CAVEWHERE_TESTLIB_EXPORT copyToTempFolder(QString filename);

/**
 * Copies a testcases dataset file to a temp folder and returns the temp path.
 */
inline QString testcasesDatasetPath(const QString& relativePath) {
    return copyToTempFolder(testcasesDatasetSourcePath(relativePath));
}

/**
 * Copies a QML test dataset file to a temp folder and returns the temp path.
 */
inline QString qmlTestDatasetPath(const QString& relativePath) {
    return copyToTempFolder(qmlTestDatasetSourcePath(relativePath));
}

QString CAVEWHERE_TESTLIB_EXPORT prependTempFolder(QString filename);

void CAVEWHERE_TESTLIB_EXPORT addTokenManager(cwProject* project);

/**
 * Creates a bare git repository at \a path with default branch "main".
 * Returns 0 on success, or a libgit2 error code on failure.
 */
int CAVEWHERE_TESTLIB_EXPORT initBareRepo(const QString& path);

/**
 * Blocks until pending saves complete, then returns true if the project has
 * local modifications or uncommitted git changes. Test-only — spins a nested
 * event loop.
 */
bool CAVEWHERE_TESTLIB_EXPORT isProjectModified(cwProject* project);

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

/**
 * Owning fixture for C++ sync tests that need a local project, a bare origin,
 * and a peer clone loaded as a second cwProject. All temp dirs and rootData
 * are owned by unique_ptr and destroyed when the fixture goes out of scope,
 * so a test can declare it as an automatic variable and get RAII cleanup.
 */
struct CAVEWHERE_TESTLIB_EXPORT cwSyncChurnFixture {
    std::unique_ptr<QTemporaryDir> projectDir;
    std::unique_ptr<QTemporaryDir> remoteRoot;
    std::unique_ptr<QTemporaryDir> cloneDir;

    std::unique_ptr<cwRootData> rootData;
    std::unique_ptr<cwRootData> remoteRootData;

    // Non-owning convenience pointers; valid for the fixture's lifetime.
    cwProject* project = nullptr;
    cwProject* remoteProject = nullptr;
    QQuickGit::GitRepository* repository = nullptr;
    QQuickGit::GitRepository* remoteRepository = nullptr;

    QString projectPath;
    QString remoteRepoPath;
    QString clonePath;

    cwSyncChurnFixture() = default;
    cwSyncChurnFixture(cwSyncChurnFixture&&) noexcept = default;
    cwSyncChurnFixture& operator=(cwSyncChurnFixture&&) noexcept = default;
    cwSyncChurnFixture(const cwSyncChurnFixture&) = delete;
    cwSyncChurnFixture& operator=(const cwSyncChurnFixture&) = delete;
    ~cwSyncChurnFixture() = default;
};

struct cwSyncChurnFixtureConfig {
    QString caveName;
    std::optional<QString> tripName;
    QString projectFileBaseName = QStringLiteral("sync-fixture.cwproj");
    bool addPngNoteFromDataset = false;
    QString cloneSubdirName = QStringLiteral("clone-2");
    QString localAccountName = QStringLiteral("Sync Tester");
    QString localAccountEmail = QStringLiteral("sync.tester@example.com");
    QString remoteAccountName = QStringLiteral("Remote Sync Tester");
    QString remoteAccountEmail = QStringLiteral("remote.sync.tester@example.com");
};

/**
 * Build the multi-peer sync fixture described by \a config. Fails the current
 * Catch2 test case via REQUIRE on any setup step error.
 */
cwSyncChurnFixture CAVEWHERE_TESTLIB_EXPORT
makeSyncChurnFixture(const cwSyncChurnFixtureConfig& config);

struct cwChurnLoopConfig {
    int budgetMs = 2000;
    int processEventsMaxMs = 20;
    int postMutateSleepMs = 0;
};

/**
 * Spin the event loop while \a futureManager still has work, calling
 * \a mutateOnce each iteration to perturb the in-memory model. Returns the
 * number of mutations applied. Template keeps the callable inline; the stress
 * path runs this loop many thousands of times.
 */
template <typename Mutator>
int runChurnLoop(cwFutureManagerModel* futureManager,
                 const cwChurnLoopConfig& config,
                 Mutator&& mutateOnce)
{
    int iterations = 0;
    QElapsedTimer timer;
    timer.start();
    while (futureManager->rowCount() > 0 && timer.elapsed() < config.budgetMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, config.processEventsMaxMs);
        mutateOnce(iterations);
        ++iterations;
        if (config.postMutateSleepMs > 0) {
            QThread::msleep(static_cast<unsigned long>(config.postMutateSleepMs));
        }
    }
    return iterations;
}

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
    Q_INVOKABLE QUrl copyToTempDirUrl(const QString& filename);

    Q_INVOKABLE bool fileExists(const QUrl& filename) const;
    Q_INVOKABLE bool copyFile(const QString& sourcePath, const QString& destPath) const;
    Q_INVOKABLE bool directoryExists(const QUrl& directory) const;
    Q_INVOKABLE size_t fileSize(const QUrl& filename) const;
    Q_INVOKABLE void removeFile(const QUrl& filename) const;
    Q_INVOKABLE void removeDirectory(const QUrl& directory) const;
    Q_INVOKABLE QString environmentVariable(const QString& name) const;
    Q_INVOKABLE QString repositoryRemoteUrl(const QUrl& repositoryDirectory,
                                            const QString& remoteName = QStringLiteral("origin")) const;
    Q_INVOKABLE QString normalizeFileGitUrl(const QString& url) const;
    Q_INVOKABLE QString projectHeadCommitOid(cwProject* project) const;
    Q_INVOKABLE QString projectHeadCommitMessage(cwProject* project) const;
    Q_INVOKABLE int projectModifiedFileCount(cwProject* project) const;
    Q_INVOKABLE void waitForProjectSaveToFinish(cwProject* project) const;
    Q_INVOKABLE void waitForFutureManagerToFinish(cwFutureManagerModel* model) const;
    Q_INVOKABLE QString checkoutProjectRef(cwProject* project,
                                           const QString& refSpec,
                                           bool force = true) const;
    Q_INVOKABLE int noteScrapCount(cwNote* note) const;
    Q_INVOKABLE QVariantMap scrapOutlineState(cwNote* note, int scrapIndex) const;
    Q_INVOKABLE bool addScrapStation(cwNote* note,
                                     int scrapIndex,
                                     const QString& name,
                                     const QPointF& positionOnNote) const;
    Q_INVOKABLE bool addScrapLead(cwNote* note,
                                  int scrapIndex,
                                  const QPointF& positionOnNote,
                                  const QSizeF& size,
                                  const QString& description) const;
    Q_INVOKABLE bool addLiDARStation(cwNoteLiDAR* note,
                                     const QString& name,
                                     const QVector3D& positionOnNote) const;
    Q_INVOKABLE int liDARStationLookupSize(cwNoteLiDAR* note) const;
    Q_INVOKABLE bool liDARSurveyNetworkIsEmpty(cwNoteLiDAR* note) const;
    Q_INVOKABLE QString firstUnusedTripStationName(cwTrip* trip,
                                                   const QStringList& excludedNames) const;
    Q_INVOKABLE cwSyncFixtureInfo createLocalSyncFixtureWithLfsServer();
    Q_INVOKABLE cwSyncFixtureInfo createLocalLiDARSyncFixtureWithLfsServer();
    Q_INVOKABLE cwCloneFixtureInfo createLocalBareRemoteForCloneTest();
    Q_INVOKABLE bool setGitHubAccessTokenForAccount(const QString& accountId,
                                                    const QString& token) const;
    Q_INVOKABLE bool clearGitHubAccessTokenForAccount(const QString& accountId) const;

    Q_INVOKABLE QString testcasesDatasetPath(const QString& relativePath) const {
        return ::testcasesDatasetPath(relativePath);
    }
    Q_INVOKABLE QString qmlTestDatasetPath(const QString& relativePath) const {
        return ::qmlTestDatasetPath(relativePath);
    }

    Q_INVOKABLE QUrl tempDirectoryUrl() {
        return QUrl::fromLocalFile(createTempSubdir());
    }

    Q_INVOKABLE QUrl toLocalUrl(const QString& path) {
        return QUrl::fromLocalFile(path);
    }

    //! True when the build links Qt Pdf; PDF note import is a no-op otherwise.
    Q_INVOKABLE bool pdfSupported() const {
        return cwPDFConverter::isSupported();
    }

    /**
     * Writes a small synthetic .laz under the shared temp directory and
     * returns its absolute path. The file name is tagged with @a tag and the
     * test process PID so concurrent test runs don't collide.
     */
    Q_INVOKABLE QString writeMinimalLazInTempDir(const QString& tag);

    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE void setClipboardText(const QString& text) const;

    Q_INVOKABLE void appendError(cwErrorListModel* model,
                                 const QString& message,
                                 int type) const;

};

#endif // LOADPROJECTHELPER
