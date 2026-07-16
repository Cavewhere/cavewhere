#include "LoadProjectHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDirIterator>
#include <QUrl>
#include <QProcessEnvironment>
#include <QVariantList>

//Our includes
#include "cwZip.h"
#include "LazFixtureHelper.h"
#include "cwFutureManagerModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwNoteLiDAR.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwScrap.h"
#include "cwLead.h"
#include "cwLinePlotManager.h"
#include "cwErrorListModel.h"
#include "cwRootData.h"
#include "GitRepository.h"
#include "Account.h"
#include "cwGitHubCredentials.h"
#include "cwRemoteCredentialStore.h"
#include "asyncfuture.h"
#include "LfsServer.h"

//libgit2
#include "git2.h"
#include <qtkeychain/keychain.h>
#include <QSignalSpy>

//Std includes
#include <atomic>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

// Catch2 REQUIRE/CHECK cannot be used from inside a shared library (DLL) on
// Windows: each DLL gets its own copy of Catch2's thread-local ResultCapture,
// so assertions in the DLL fire against a null context and throw
// "No result capture instance".  Use FIXTURE_REQUIRE instead — it throws
// std::runtime_error, which the TEST_CASE catches and reports as a failure
// with a descriptive message.
#define FIXTURE_REQUIRE(cond) \
    do { \
        if (!(cond)) \
            throw std::runtime_error("Fixture setup failed: " #cond); \
    } while (false)

namespace {
std::unique_ptr<LfsServer> g_syncLfsServer;
std::atomic<int> g_tempSubdirCounter{0};

// Stored as std::string so the atexit handler can read it without
// re-entering sharedTempRoot()'s function-local static.
std::string g_sharedTempRootPath;

bool setRepositoryConfigString(const QString& repositoryPath,
                               const QString& key,
                               const QString& value,
                               QString* errorMessageOut = nullptr)
{
    git_repository* repository = nullptr;
    if (git_repository_open(&repository, repositoryPath.toLocal8Bit().constData()) != GIT_OK || repository == nullptr) {
        const git_error* error = git_error_last();
        if (errorMessageOut) {
            *errorMessageOut = error ? QString::fromUtf8(error->message)
                                     : QStringLiteral("Failed to open repository config.");
        }
        return false;
    }

    git_config* config = nullptr;
    if (git_repository_config(&config, repository) != GIT_OK || config == nullptr) {
        const git_error* error = git_error_last();
        if (errorMessageOut) {
            *errorMessageOut = error ? QString::fromUtf8(error->message)
                                     : QStringLiteral("Failed to open repository config.");
        }
        git_repository_free(repository);
        return false;
    }

    const int setResult = git_config_set_string(config,
                                                key.toLocal8Bit().constData(),
                                                value.toLocal8Bit().constData());
    if (setResult != GIT_OK && errorMessageOut) {
        const git_error* error = git_error_last();
        *errorMessageOut = error ? QString::fromUtf8(error->message)
                                 : QStringLiteral("Failed setting git config value.");
    }

    git_config_free(config);
    git_repository_free(repository);
    return setResult == GIT_OK;
}
} // namespace

QString sharedTempRoot()
{
    static bool initialized = []() {
        QString path = QDir::tempPath()
                       + QStringLiteral("/cavewhere-test-")
                       + QString::number(QCoreApplication::applicationPid());
        QDir().mkpath(path);
        g_sharedTempRootPath = path.toStdString();
        std::atexit([]() {
            QDir(QString::fromStdString(g_sharedTempRootPath)).removeRecursively();
        });
        return true;
    }();
    Q_UNUSED(initialized);
    return QString::fromStdString(g_sharedTempRootPath);
}

QString createTempSubdir()
{
    QString subdir = sharedTempRoot()
                     + QStringLiteral("/")
                     + QString::number(g_tempSubdirCounter.fetch_add(1));
    QDir().mkpath(subdir);
    return subdir;
}

std::shared_ptr<cwProject> fileToProject(QString filename) {
    auto project = std::make_shared<cwProject>();
    addTokenManager(project.get());
    fileToProject(project.get(), filename);
    return project;
}

QString fileToProject(cwProject *project, const QString &filename) {
    QString datasetFile = copyToTempFolder(filename);
    project->newProject();
    project->loadOrConvert(datasetFile);
    project->waitLoadToFinish();

    return datasetFile;
}

QString copyToTempFolder(QString filename) {

    QFileInfo info(filename);
    QString newFileLocation = createTempSubdir() + "/" + info.fileName();

    if(!info.exists(filename)) {
        qFatal() << "file doesnt' exist:" << filename;
    }

    if(QFileInfo::exists(newFileLocation)) {
        QFile file(newFileLocation);
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadUser);

        bool couldRemove = file.remove();
        if(!couldRemove) {
            qFatal() << "Trying to remove " << newFileLocation;
        }
    }

    bool couldCopy = QFile::copy(filename, newFileLocation);
    if(!couldCopy) {
        qFatal() << "Trying to copy " << filename << " to " << newFileLocation;
    }

    bool couldPermissions = QFile::setPermissions(newFileLocation, QFile::WriteOwner | QFile::ReadOwner);
    if(!couldPermissions) {
        qFatal() << "Trying to set permissions for " << filename;
    }

    return newFileLocation;
}

QString prependTempFolder(QString filename)
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + filename;
}

void TestHelper::loadProjectFromZip(cwProject *project, const QString &filename) {
    QString datasetFileZip = copyToTempFolder(filename);

    QFileInfo info(datasetFileZip);
    auto result = cwZip::extractAll(datasetFileZip, info.canonicalPath());

    auto findProjectFile = [&](const QStringList& patterns) {
        QDirIterator it(info.canonicalPath(), patterns, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QFileInfo fileInfo(filePath);
            if (filePath.contains(QStringLiteral("__MACOSX")) || fileInfo.fileName().startsWith("._")) {
                continue;
            }

            return filePath;
        }
        return QString();
    };

    // Find the first .cwproj (preferred), fallback to .cw for legacy zips.
    QString projectFilePath;
    projectFilePath = findProjectFile(QStringList() << "*.cwproj");
    if (projectFilePath.isEmpty()) {
        projectFilePath = findProjectFile(QStringList() << "*.cw");
    }

    if (!projectFilePath.isEmpty()) {
        project->loadOrConvert(projectFilePath);
        project->waitLoadToFinish();
    } else {
        qFatal() << "No project file found in:" << info.canonicalPath();
    }

}

void TestHelper::loadProjectFromPath(cwProject* project, const QString& localPath)
{
    if (project == nullptr) {
        return;
    }

    project->newProject();
    project->loadOrConvert(localPath);
    project->waitLoadToFinish();

    if (auto* repository = project->repository()) {
        auto* account = repository->account();
        if (account == nullptr || !account->isValid()) {
            auto* fixtureAccount = new QQuickGit::Account(project);
            fixtureAccount->setName(QStringLiteral("QML Sync Fixture"));
            fixtureAccount->setEmail(QStringLiteral("qml.sync.fixture@example.com"));
            project->setGitAccount(fixtureAccount);
        }
    }
}

QString TestHelper::copyToTempDir(const QString &filename)
{
    return copyToTempFolder(filename);
}

QString TestHelper::writeMinimalLazInTempDir(const QString &tag)
{
    const QString path = QStringLiteral("%1/%2-%3.laz")
                             .arg(sharedTempRoot())
                             .arg(tag)
                             .arg(QCoreApplication::applicationPid());
    return writeMinimalLaz(path);
}

QUrl TestHelper::copyToTempDirUrl(const QString &filename)
{
    return QUrl::fromLocalFile(copyToTempFolder(filename));
}

bool TestHelper::fileExists(const QUrl &filename) const
{
    QFileInfo info(filename.toLocalFile());
    return info.exists();
}

bool TestHelper::copyFile(const QString &sourcePath, const QString &destPath) const
{
    QFile::remove(destPath);
    return QFile::copy(sourcePath, destPath);
}

QString TestHelper::clipboardText() const
{
    return QGuiApplication::clipboard()->text();
}

void TestHelper::setClipboardText(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

void TestHelper::appendError(cwErrorListModel *model,
                             const QString &message,
                             int type) const
{
    if (!model) {
        return;
    }
    cwError error;
    error.setMessage(message);
    error.setType(static_cast<cwError::ErrorType>(type));
    model->append(error);
}

size_t TestHelper::fileSize(const QUrl &filename) const
{
    QFileInfo info(filename.toLocalFile());
    return info.size();
}

void TestHelper::removeFile(const QUrl &filename) const
{
    QFile::remove(filename.toLocalFile());
}

void TestHelper::removeDirectory(const QUrl &directory) const
{
    QDir(directory.toLocalFile()).removeRecursively();
}

QString TestHelper::environmentVariable(const QString& name) const
{
    return QProcessEnvironment::systemEnvironment().value(name);
}

QString TestHelper::repositoryRemoteUrl(const QUrl& repositoryDirectory,
                                        const QString& remoteName) const
{
    const QString repoPath = repositoryDirectory.toLocalFile();
    if (repoPath.isEmpty()) {
        return QString();
    }

    git_repository* repository = nullptr;
    if (git_repository_open(&repository, repoPath.toLocal8Bit().constData()) != GIT_OK || repository == nullptr) {
        return QString();
    }

    git_remote* remote = nullptr;
    const int remoteResult = git_remote_lookup(&remote,
                                               repository,
                                               remoteName.toLocal8Bit().constData());
    QString url;
    if (remoteResult == GIT_OK && remote != nullptr) {
        const char* remoteUrl = git_remote_url(remote);
        if (remoteUrl != nullptr) {
            url = QString::fromUtf8(remoteUrl);
        }
    }

    if (remote) {
        git_remote_free(remote);
    }
    git_repository_free(repository);
    return url;
}

QString TestHelper::normalizeFileGitUrl(const QString& url) const
{
    const QUrl asUrl(url);
    if (asUrl.isLocalFile()) {
        return QFileInfo(asUrl.toLocalFile()).canonicalFilePath();
    }
    return url.trimmed();
}

bool TestHelper::directoryExists(const QUrl& directory) const
{
    return QDir(directory.toLocalFile()).exists();
}

QString TestHelper::projectHeadCommitOid(cwProject* project) const
{
    if (project == nullptr || project->repository() == nullptr) {
        return QString();
    }

    const QString repoPath = project->repository()->directory().absolutePath();
    const auto headResult = QQuickGit::GitRepository::headCommitOid(repoPath);
    if (headResult.hasError()) {
        return QString();
    }
    return headResult.value();
}

QString TestHelper::projectHeadCommitMessage(cwProject* project) const
{
    if (project == nullptr || project->repository() == nullptr) {
        return QString();
    }

    const QString repoPath = project->repository()->directory().absolutePath();
    const auto result = QQuickGit::GitRepository::headCommitMessage(repoPath);
    if (result.hasError()) {
        return QString();
    }
    return result.value();
}

int TestHelper::projectModifiedFileCount(cwProject* project) const
{
    if (project == nullptr || project->repository() == nullptr) {
        return -1;
    }

    project->repository()->checkStatus();
    return project->repository()->modifiedFileCount();
}

void TestHelper::waitForProjectSaveToFinish(cwProject* project) const
{
    if (project == nullptr) {
        return;
    }

    project->waitSaveToFinish();
}

void TestHelper::waitForFutureManagerToFinish(cwFutureManagerModel* model) const
{
    if (model == nullptr) {
        return;
    }

    model->waitForFinished();
}

QString TestHelper::checkoutProjectRef(cwProject* project,
                                       const QString& refSpec,
                                       bool force) const
{
    if (project == nullptr) {
        return QStringLiteral("Project is null.");
    }

    const int errorCountBefore = project->errorModel() ? project->errorModel()->count() : 0;
    auto resetMode = force
                         ? cwProject::BranchResetMode::Hard
                         : cwProject::BranchResetMode::Mixed;
    if (!project->resetBranchAndReconcile(refSpec, resetMode)) {
        return QStringLiteral("Failed to start reset-branch and reconcile.");
    }

    project->waitForSyncToFinish();

    if (project->syncInProgress()) {
        return QStringLiteral("Timed out waiting for reset-branch and reconcile.");
    }

    if (project->errorModel() && project->errorModel()->count() > errorCountBefore) {
        const cwError lastError = project->errorModel()->last();
        if (!lastError.message().isEmpty()) {
            return lastError.message();
        }
        return QStringLiteral("Reset-branch and reconcile reported an error.");
    }

    return QString();
}

int TestHelper::noteScrapCount(cwNote* note) const
{
    if (note == nullptr) {
        return -1;
    }
    return note->scraps().size();
}

QVariantMap TestHelper::scrapOutlineState(cwNote* note, int scrapIndex) const
{
    if (note == nullptr || scrapIndex < 0 || scrapIndex >= note->scraps().size()) {
        return {};
    }

    const cwScrap* scrap = note->scrap(scrapIndex);
    if (scrap == nullptr) {
        return {};
    }

    QVariantList points;
    const QVector<QPointF> scrapPoints = scrap->points();
    for (int i = 0; i < scrapPoints.size(); ++i) {
        const QPointF& point = scrapPoints.at(i);
        QVariantMap pointObject;
        pointObject.insert(QStringLiteral("index"), i);
        pointObject.insert(QStringLiteral("x"), point.x());
        pointObject.insert(QStringLiteral("y"), point.y());
        points.append(pointObject);
    }

    QVariantMap state;
    state.insert(QStringLiteral("pointCount"), scrap->numberOfPoints());
    state.insert(QStringLiteral("points"), points);
    return state;
}

bool TestHelper::addScrapStation(cwNote* note,
                                 int scrapIndex,
                                 const QString& name,
                                 const QPointF& positionOnNote) const
{
    if (note == nullptr) {
        return false;
    }

    cwScrap* scrap = note->scrap(scrapIndex);
    if (scrap == nullptr) {
        return false;
    }

    cwNoteStation station;
    station.setName(name);
    station.setPositionOnNote(positionOnNote);
    scrap->addStation(station);
    return true;
}

bool TestHelper::addScrapLead(cwNote* note,
                              int scrapIndex,
                              const QPointF& positionOnNote,
                              const QSizeF& size,
                              const QString& description) const
{
    if (note == nullptr) {
        return false;
    }

    cwScrap* scrap = note->scrap(scrapIndex);
    if (scrap == nullptr) {
        return false;
    }

    cwLead lead;
    lead.setPositionOnNote(positionOnNote);
    lead.setSize(size);
    lead.setDescription(description);
    scrap->addLead(lead);
    return true;
}

bool TestHelper::addLiDARStation(cwNoteLiDAR* note,
                                 const QString& name,
                                 const QVector3D& positionOnNote) const
{
    if (note == nullptr) {
        return false;
    }

    cwNoteLiDARStation station;
    station.setName(name);
    station.setPositionOnNote(positionOnNote);
    note->addStation(station);
    return true;
}

int TestHelper::liDARStationLookupSize(cwNoteLiDAR* note) const
{
    if (note == nullptr) {
        return 0;
    }

    cwCave* cave = note->parentCave();
    if (cave == nullptr) {
        return 0;
    }

    return cave->stationPositionLookup().positions().size();
}

bool TestHelper::liDARSurveyNetworkIsEmpty(cwNoteLiDAR* note) const
{
    if (note == nullptr) {
        return true;
    }

    cwCave* cave = note->parentCave();
    if (cave == nullptr) {
        return true;
    }

    return cave->network().isEmpty();
}

QString TestHelper::firstUnusedTripStationName(cwTrip* trip,
                                               const QStringList& excludedNames) const
{
    if (trip == nullptr) {
        return QString();
    }

    QSet<QString> excluded;
    for (const QString& name : excludedNames) {
        excluded.insert(name.trimmed().toLower());
    }

    const QList<cwStation> tripStations = trip->uniqueStations();
    for (const cwStation& station : tripStations) {
        const QString name = station.name().trimmed();
        if (!name.isEmpty() && !excluded.contains(name.toLower())) {
            return name;
        }
    }

    return QString();
}

cwSyncFixtureInfo TestHelper::createLocalSyncFixtureWithLfsServer()
{
    cwSyncFixtureInfo result;

    if (!g_syncLfsServer) {
        g_syncLfsServer = std::make_unique<LfsServer>();
        if (!g_syncLfsServer->start()) {
            result.errorMessage = QStringLiteral("Failed to start LFS test server.");
            return result;
        }
    }
    const QString lfsEndpoint = g_syncLfsServer->endpoint();

    QTemporaryDir remoteRoot;
    remoteRoot.setAutoRemove(false);
    QTemporaryDir workingRoot;
    workingRoot.setAutoRemove(false);

    if (!remoteRoot.isValid() || !workingRoot.isValid()) {
        result.errorMessage = QStringLiteral("Failed to create one or more temporary directories for sync fixture.");
        return result;
    }

    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("sync-fixture.git"));
    if (initBareRepo(remoteRepoPath) != GIT_OK) {
        const git_error* error = git_error_last();
        result.errorMessage = error ? QString::fromUtf8(error->message)
                                    : QStringLiteral("Failed to initialize bare repository.");
        return result;
    }

    QQuickGit::Account account;
    account.setName(QStringLiteral("QML Sync Fixture"));
    account.setEmail(QStringLiteral("qml.sync.fixture@example.com"));

    cwProject project;
    addTokenManager(&project);
    project.setGitAccount(&account);
    const QString datasetFile = testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw");
    project.loadOrConvert(datasetFile);
    project.waitLoadToFinish();

    auto* region = project.cavingRegion();
    if (region == nullptr) {
        result.errorMessage = QStringLiteral("Failed to load fixture project data.");
        return result;
    }
    if (region->caveCount() <= 0) {
        region->addCave();
    }

    auto* cave = region->cave(0);
    if (cave == nullptr) {
        result.errorMessage = QStringLiteral("Failed to access fixture cave.");
        return result;
    }
    if (cave->tripCount() <= 0) {
        cave->addTrip();
    }

    const QString requestedProjectPath = QDir(workingRoot.path()).filePath(QStringLiteral("sync-fixture.cwproj"));
    if (!project.saveAs(requestedProjectPath)) {
        result.errorMessage = QStringLiteral("Failed to save sync fixture project.");
        return result;
    }
    project.waitSaveToFinish();
    const QString projectPath = project.filename();

    QQuickGit::GitRepository* repository = project.repository();
    if (repository == nullptr) {
        result.errorMessage = QStringLiteral("Failed to access fixture repository.");
        return result;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    if (!addRemoteError.isEmpty()) {
        result.errorMessage = addRemoteError;
        return result;
    }

    const QString workingRepoPath = repository->directory().absolutePath();
    QString configError;
    if (!setRepositoryConfigString(workingRepoPath, QStringLiteral("lfs.url"), lfsEndpoint, &configError)
        || !setRepositoryConfigString(workingRepoPath,
                                      QStringLiteral("remote.origin.lfsurl"),
                                      lfsEndpoint,
                                      &configError)
        || !setRepositoryConfigString(workingRepoPath,
                                      QStringLiteral("user.name"),
                                      QStringLiteral("QML Sync Fixture"),
                                      &configError)
        || !setRepositoryConfigString(workingRepoPath,
                                      QStringLiteral("user.email"),
                                      QStringLiteral("qml.sync.fixture@example.com"),
                                      &configError)) {
        result.errorMessage = configError.isEmpty()
                                  ? QStringLiteral("Failed to configure LFS endpoint for sync fixture.")
                                  : configError;
        return result;
    }

    QFile seedFile(repository->directory().filePath(QStringLiteral("README.md")));
    if (!seedFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        result.errorMessage = QStringLiteral("Failed to write sync fixture seed file.");
        return result;
    }
    seedFile.write("sync fixture\n");
    seedFile.close();

    try {
        repository->commitAll(QStringLiteral("Seed sync fixture"),
                              QStringLiteral("Seed repository for QML sync tests"));
    } catch (const std::exception& ex) {
        result.errorMessage = QStringLiteral("Failed to create seed commit: %1")
                                  .arg(QString::fromUtf8(ex.what()));
        return result;
    }

    const auto pushFuture = repository->push();
    if (!AsyncFuture::waitForFinished(pushFuture, 15000)) {
        result.errorMessage = QStringLiteral("Timed out waiting for fixture push.");
        return result;
    }
    const auto pushResult = pushFuture.result();
    if (pushResult.hasError()) {
        result.errorMessage = pushResult.errorMessage();
        return result;
    }

    result.errorMessage = QString();
    result.projectFilePath = projectPath;
    result.workingRepoPath = workingRepoPath;
    result.remoteRepoPath = remoteRepoPath;
    result.branchName = repository->headBranchName();
    result.lfsEndpoint = lfsEndpoint;
    return result;
}

cwSyncFixtureInfo TestHelper::createLocalLiDARSyncFixtureWithLfsServer()
{
    cwSyncFixtureInfo result;

    if (!g_syncLfsServer) {
        g_syncLfsServer = std::make_unique<LfsServer>();
        if (!g_syncLfsServer->start()) {
            result.errorMessage = QStringLiteral("Failed to start LFS test server.");
            return result;
        }
    }
    const QString lfsEndpoint = g_syncLfsServer->endpoint();

    QTemporaryDir remoteRoot;
    remoteRoot.setAutoRemove(false);
    QTemporaryDir workingRoot;
    workingRoot.setAutoRemove(false);

    if (!remoteRoot.isValid() || !workingRoot.isValid()) {
        result.errorMessage = QStringLiteral("Failed to create one or more temporary directories for sync fixture.");
        return result;
    }

    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("sync-fixture.git"));
    if (initBareRepo(remoteRepoPath) != GIT_OK) {
        const git_error* error = git_error_last();
        result.errorMessage = error ? QString::fromUtf8(error->message)
                                    : QStringLiteral("Failed to initialize bare repository.");
        return result;
    }

    QQuickGit::Account account;
    account.setName(QStringLiteral("QML Sync Fixture"));
    account.setEmail(QStringLiteral("qml.sync.fixture@example.com"));

    cwProject project;
    addTokenManager(&project);
    project.setGitAccount(&account);

    const QString datasetFileZip = testcasesDatasetPath("lidarProjects/jaws of the beast.zip");
    QFileInfo zipInfo(datasetFileZip);
    cwZip::extractAll(datasetFileZip, zipInfo.canonicalPath());

    QString datasetProjectFile;
    QDirIterator it(zipInfo.canonicalPath(),
                    QStringList() << "*.cwproj" << "*.cw",
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        if (filePath.contains(QStringLiteral("__MACOSX")) || fileInfo.fileName().startsWith("._")) {
            continue;
        }
        datasetProjectFile = filePath;
        break;
    }

    if (datasetProjectFile.isEmpty()) {
        result.errorMessage = QStringLiteral("Failed to find LiDAR fixture project file.");
        return result;
    }

    project.loadOrConvert(datasetProjectFile);
    project.waitLoadToFinish();

    auto* region = project.cavingRegion();
    if (region == nullptr || region->caveCount() <= 0) {
        result.errorMessage = QStringLiteral("Failed to load LiDAR fixture project data.");
        return result;
    }

    auto* cave = region->cave(0);
    if (cave == nullptr || cave->tripCount() <= 0) {
        result.errorMessage = QStringLiteral("Failed to access LiDAR fixture cave.");
        return result;
    }

    auto* trip = cave->trip(0);
    if (trip == nullptr || trip->notesLiDAR() == nullptr) {
        result.errorMessage = QStringLiteral("Failed to access LiDAR notes model for fixture.");
        return result;
    }

    const QString requestedProjectPath = QDir(workingRoot.path()).filePath(QStringLiteral("sync-fixture.cwproj"));
    if (!project.saveAs(requestedProjectPath)) {
        result.errorMessage = QStringLiteral("Failed to save sync fixture project.");
        return result;
    }
    project.waitSaveToFinish();
    const QString projectPath = project.filename();
    const QString nestedGitDirPath = QDir(project.dataRootDir()).filePath(QStringLiteral(".git"));
    if (QFileInfo::exists(nestedGitDirPath)) {
        QDir nestedGitDir(nestedGitDirPath);
        if (!nestedGitDir.removeRecursively()) {
            result.errorMessage = QStringLiteral("Failed to remove nested git directory from LiDAR sync fixture.");
            return result;
        }
    }

    const QString sourceGlbPath = testcasesDatasetPath("lidarProjects/9_15_2025 3.glb");
    const QFileInfo sourceGlbInfo(sourceGlbPath);
    const QString lidarFileName = sourceGlbInfo.fileName();
    const QString destinationGlbPath = project.notesDir(trip->notesLiDAR()).filePath(lidarFileName);
    if (QFileInfo::exists(destinationGlbPath) && !QFile::remove(destinationGlbPath)) {
        result.errorMessage = QStringLiteral("Failed to replace existing LiDAR fixture file.");
        return result;
    }
    if (!QFile::copy(sourceGlbPath, destinationGlbPath)) {
        result.errorMessage = QStringLiteral("Failed to seed LiDAR fixture GLB file.");
        return result;
    }

    auto* lidarNote = new cwNoteLiDAR(trip->notesLiDAR());
    lidarNote->setParentTrip(trip);
    lidarNote->setName(lidarFileName);
    lidarNote->setFilename(lidarFileName);

    cwNoteLiDARStation stationA;
    stationA.setName(QStringLiteral("6"));
    stationA.setPositionOnNote(QVector3D(-0.25f, 0.0f, 0.0f));
    lidarNote->addStation(stationA);

    cwNoteLiDARStation stationB;
    stationB.setName(QStringLiteral("7"));
    stationB.setPositionOnNote(QVector3D(0.25f, 0.0f, 0.0f));
    lidarNote->addStation(stationB);

    trip->notesLiDAR()->addNotes({lidarNote});

    if (!project.save()) {
        result.errorMessage = QStringLiteral("Failed to save LiDAR sync fixture project.");
        return result;
    }
    project.waitSaveToFinish();

    QQuickGit::GitRepository* repository = project.repository();
    if (repository == nullptr) {
        result.errorMessage = QStringLiteral("Failed to access fixture repository.");
        return result;
    }

    const QString addRemoteError = repository->addRemote(QStringLiteral("origin"),
                                                         QUrl::fromLocalFile(remoteRepoPath));
    if (!addRemoteError.isEmpty()) {
        result.errorMessage = addRemoteError;
        return result;
    }

    const QString workingRepoPath = repository->directory().absolutePath();
    QString configError;
    if (!setRepositoryConfigString(workingRepoPath, QStringLiteral("lfs.url"), lfsEndpoint, &configError)
        || !setRepositoryConfigString(workingRepoPath,
                                      QStringLiteral("remote.origin.lfsurl"),
                                      lfsEndpoint,
                                      &configError)
        || !setRepositoryConfigString(workingRepoPath,
                                      QStringLiteral("user.name"),
                                      QStringLiteral("QML Sync Fixture"),
                                      &configError)
        || !setRepositoryConfigString(workingRepoPath,
                                      QStringLiteral("user.email"),
                                      QStringLiteral("qml.sync.fixture@example.com"),
                                      &configError)) {
        result.errorMessage = configError.isEmpty()
                                  ? QStringLiteral("Failed to configure LFS endpoint for sync fixture.")
                                  : configError;
        return result;
    }

    QFile seedFile(repository->directory().filePath(QStringLiteral("README.md")));
    if (!seedFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        result.errorMessage = QStringLiteral("Failed to write sync fixture seed file.");
        return result;
    }
    seedFile.write("sync fixture\n");
    seedFile.close();

    try {
        repository->commitAll(QStringLiteral("Seed LiDAR sync fixture"),
                              QStringLiteral("Seed repository for LiDAR QML sync tests"));
    } catch (const std::exception& ex) {
        result.errorMessage = QStringLiteral("Failed to create seed commit: %1")
                                  .arg(QString::fromUtf8(ex.what()));
        return result;
    }

    const auto pushFuture = repository->push();
    if (!AsyncFuture::waitForFinished(pushFuture, 15000)) {
        result.errorMessage = QStringLiteral("Timed out waiting for fixture push.");
        return result;
    }
    const auto pushResult = pushFuture.result();
    if (pushResult.hasError()) {
        result.errorMessage = pushResult.errorMessage();
        return result;
    }

    result.errorMessage = QString();
    result.projectFilePath = projectPath;
    result.workingRepoPath = workingRepoPath;
    result.remoteRepoPath = remoteRepoPath;
    result.branchName = repository->headBranchName();
    result.lfsEndpoint = lfsEndpoint;
    return result;
}

cwCloneFixtureInfo TestHelper::createLocalBareRemoteForCloneTest()
{
    cwCloneFixtureInfo result;

    QTemporaryDir remoteRoot;
    remoteRoot.setAutoRemove(false);
    QTemporaryDir authorRoot;
    authorRoot.setAutoRemove(false);
    QTemporaryDir cloneParentRoot;
    cloneParentRoot.setAutoRemove(false);

    if (!remoteRoot.isValid() || !authorRoot.isValid() || !cloneParentRoot.isValid()) {
        result.errorMessage = QStringLiteral("Failed to create one or more temporary directories for clone fixture.");
        return result;
    }

    const QString remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("clone-fixture.git"));
    if (initBareRepo(remoteRepoPath) != GIT_OK) {
        const git_error* error = git_error_last();
        result.errorMessage = error ? QString::fromUtf8(error->message)
                                    : QStringLiteral("Failed to initialize bare repository.");
        return result;
    }

    const QString authorRepoPath = QDir(authorRoot.path()).filePath(QStringLiteral("author"));
    if (!QDir().mkpath(authorRepoPath)) {
        result.errorMessage = QStringLiteral("Failed to create author repository directory.");
        return result;
    }

    QQuickGit::Account account;
    account.setName(QStringLiteral("QML Clone Fixture"));
    account.setEmail(QStringLiteral("qml.clone.fixture@example.com"));

    cwProject authorProject;
    addTokenManager(&authorProject);
    authorProject.setGitAccount(&account);

    auto* region = authorProject.cavingRegion();
    if (region == nullptr) {
        result.errorMessage = QStringLiteral("Failed to create caving region for clone fixture.");
        return result;
    }
    region->addCave();
    auto* cave = region->cave(0);
    if (cave == nullptr) {
        result.errorMessage = QStringLiteral("Failed to create cave for clone fixture.");
        return result;
    }
    cave->setName(QStringLiteral("Clone Fixture Cave"));

    const QString projectPath = QDir(authorRepoPath).filePath(QStringLiteral("clone-fixture.cwproj"));
    if (!authorProject.saveAs(projectPath)) {
        result.errorMessage = QStringLiteral("Failed to save clone fixture project.");
        return result;
    }
    authorProject.waitSaveToFinish();

    QQuickGit::GitRepository* authorRepository = authorProject.repository();
    if (authorRepository == nullptr) {
        result.errorMessage = QStringLiteral("Failed to access author repository for clone fixture.");
        return result;
    }

    const QDir repositoryDir = authorRepository->directory();
    if (!repositoryDir.exists()) {
        result.errorMessage = QStringLiteral("Fixture repository directory does not exist.");
        return result;
    }

    // Ensure there is at least one commit to push from this fixture repository.
    QFile seedFile(repositoryDir.filePath(QStringLiteral("README.md")));
    if (!seedFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        result.errorMessage = QStringLiteral("Failed to write seed README for clone fixture.");
        return result;
    }
    seedFile.write("clone fixture\n");
    seedFile.close();

    try {
        authorRepository->commitAll(QStringLiteral("Seed clone fixture"),
                                    QStringLiteral("Seed clone fixture repository for QML tests"));
    } catch (const std::exception& ex) {
        result.errorMessage = QStringLiteral("Failed to create seed commit: %1")
                                  .arg(QString::fromUtf8(ex.what()));
        return result;
    }

    const QFileInfoList topLevelFiles = repositoryDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    bool hasTopLevelProject = false;
    for (const QFileInfo& fileInfo : topLevelFiles) {
        if (fileInfo.suffix().compare(QStringLiteral("cwproj"), Qt::CaseInsensitive) == 0) {
            hasTopLevelProject = true;
            break;
        }
    }
    if (!hasTopLevelProject) {
        const QString sourceProjectPath = authorProject.filename();
        const QString fallbackProjectPath = repositoryDir.filePath(QStringLiteral("clone-fixture.cwproj"));
        if (!QFileInfo::exists(sourceProjectPath) || !QFile::copy(sourceProjectPath, fallbackProjectPath)) {
            result.errorMessage = QStringLiteral("Failed to ensure top-level CaveWhere project file in fixture repo.");
            return result;
        }
        try {
            authorRepository->commitAll(QStringLiteral("Add fixture project file"),
                                        QStringLiteral("Ensure top-level project file for openRepository"));
        } catch (const std::exception& ex) {
            result.errorMessage = QStringLiteral("Failed to commit top-level project file: %1")
                                      .arg(QString::fromUtf8(ex.what()));
            return result;
        }
    }

    const QString addRemoteError = authorRepository->addRemote(QStringLiteral("origin"),
                                                               QUrl::fromLocalFile(remoteRepoPath));
    if (!addRemoteError.isEmpty()) {
        result.errorMessage = addRemoteError;
        return result;
    }

    const auto pushFuture = authorRepository->push();
    if (!AsyncFuture::waitForFinished(pushFuture, 15000)) {
        result.errorMessage = QStringLiteral("Timed out waiting for fixture push.");
        return result;
    }
    const auto pushResult = pushFuture.result();
    if (pushResult.hasError()) {
        result.errorMessage = pushResult.errorMessage();
        return result;
    }

    const QString repoName = QFileInfo(remoteRepoPath).completeBaseName();
    const QString expectedClonePath = QDir(cloneParentRoot.path()).filePath(repoName);

    result.errorMessage = QString();
    result.remoteUrl = QUrl::fromLocalFile(remoteRepoPath).toString();
    result.remoteRepoPath = remoteRepoPath;
    result.cloneParentPath = cloneParentRoot.path();
    result.repoName = repoName;
    result.expectedClonePath = expectedClonePath;
    return result;
}

bool TestHelper::setGitHubAccessTokenForAccount(const QString& accountId,
                                                const QString& token) const
{
    const QString key = cwRemoteCredentialStore::credentialBlobKey(cwRemoteAccountModel::Provider::GitHub,
                                                                    accountId);
    if (key.isEmpty()) {
        return false;
    }

    cwGitHubCredentials credentials;
    credentials.accessToken = token;
    credentials.refreshToken = QStringLiteral("cw-test-refresh-token");

    QKeychain::WritePasswordJob job(QStringLiteral("CaveWhere"));
    job.setAutoDelete(false);
    job.setKey(key);
    job.setBinaryData(credentials.toKeychainBytes());
    QSignalSpy spy(&job, &QKeychain::Job::finished);
    job.start();
    if (!spy.wait(5000)) {
        return false;
    }

    return job.error() == QKeychain::NoError;
}

bool TestHelper::clearGitHubAccessTokenForAccount(const QString& accountId) const
{
    const QString key = cwRemoteCredentialStore::credentialBlobKey(cwRemoteAccountModel::Provider::GitHub,
                                                                    accountId);
    if (key.isEmpty()) {
        return false;
    }

    QKeychain::DeletePasswordJob job(QStringLiteral("CaveWhere"));
    job.setAutoDelete(false);
    job.setKey(key);
    QSignalSpy spy(&job, &QKeychain::Job::finished);
    job.start();
    if (!spy.wait(5000)) {
        return false;
    }

    return job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound;
}



void addTokenManager(cwProject *project)
{
    cwFutureManagerModel* model = new cwFutureManagerModel(project);
    project->setFutureManagerToken(model->token());
}

bool isProjectModified(cwProject* project)
{
    project->waitSaveToFinish();
    if (project->modified()) {
        return true;
    }
    auto* repo = project->repository();
    if (repo == nullptr) {
        return false;
    }
    repo->checkStatus();
    return repo->modifiedFileCount() > 0;
}

int initBareRepo(const QString& path)
{
    git_repository* repo = nullptr;
    git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;
    opts.flags = GIT_REPOSITORY_INIT_BARE | GIT_REPOSITORY_INIT_MKPATH;
    opts.initial_head = "main";
    int err = git_repository_init_ext(&repo, path.toLocal8Bit().constData(), &opts);
    if (repo) {
        git_repository_free(repo);
    }
    return err;
}

cwSyncChurnFixture makeSyncChurnFixture(const cwSyncChurnFixtureConfig& config)
{
    cwSyncChurnFixture fx;

    fx.rootData = std::make_unique<cwRootData>();
    fx.project = fx.rootData->project();
    FIXTURE_REQUIRE(fx.project != nullptr);

    fx.rootData->account()->setName(config.localAccountName);
    fx.rootData->account()->setEmail(config.localAccountEmail);

    auto* region = fx.project->cavingRegion();
    FIXTURE_REQUIRE(region != nullptr);
    region->addCave();
    auto* cave = region->cave(0);
    FIXTURE_REQUIRE(cave != nullptr);
    cave->setName(config.caveName);

    cwTrip* trip = nullptr;
    if (config.tripName.has_value()) {
        cave->addTrip();
        trip = cave->trip(0);
        FIXTURE_REQUIRE(trip != nullptr);
        trip->setName(*config.tripName);
    }

    fx.projectDir = std::make_unique<QTemporaryDir>();
    FIXTURE_REQUIRE(fx.projectDir->isValid());
    fx.projectPath = QDir(fx.projectDir->path()).filePath(config.projectFileBaseName);
    FIXTURE_REQUIRE(fx.project->saveAs(fx.projectPath));
    fx.project->waitSaveToFinish();

    if (config.addPngNoteFromDataset) {
        FIXTURE_REQUIRE(trip != nullptr);
        const QString pngSource = copyToTempFolder(
            testcasesDatasetPath(QStringLiteral("test_cwTextureUploadTask/PhakeCave.PNG")));
        FIXTURE_REQUIRE(QFileInfo::exists(pngSource));
        trip->notes()->addFromFiles({QUrl::fromLocalFile(pngSource)});
        fx.rootData->futureManagerModel()->waitForFinished();
        fx.project->waitSaveToFinish();
        FIXTURE_REQUIRE(trip->notes()->rowCount() == 1);
    }

    fx.repository = fx.project->repository();
    FIXTURE_REQUIRE(fx.repository != nullptr);

    fx.remoteRoot = std::make_unique<QTemporaryDir>();
    FIXTURE_REQUIRE(fx.remoteRoot->isValid());
    fx.remoteRepoPath = QDir(fx.remoteRoot->path()).filePath(QStringLiteral("remote.git"));
    FIXTURE_REQUIRE(initBareRepo(fx.remoteRepoPath) == GIT_OK);

    const QString addRemoteError = fx.repository->addRemote(
        QStringLiteral("origin"), QUrl::fromLocalFile(fx.remoteRepoPath));
    FIXTURE_REQUIRE(addRemoteError.isEmpty());

    fx.project->errorModel()->clear();
    FIXTURE_REQUIRE(fx.project->sync());
    fx.rootData->futureManagerModel()->waitForFinished();
    fx.project->waitSaveToFinish();
    FIXTURE_REQUIRE(fx.project->errorModel()->count() == 0);

    fx.cloneDir = std::make_unique<QTemporaryDir>();
    FIXTURE_REQUIRE(fx.cloneDir->isValid());
    fx.clonePath = QDir(fx.cloneDir->path()).filePath(config.cloneSubdirName);

    QQuickGit::GitRepository cloneRepository;
    cloneRepository.setDirectory(QDir(fx.clonePath));
    cloneRepository.setAccount(fx.rootData->account());

    auto cloneFuture = cloneRepository.clone(QUrl::fromLocalFile(fx.remoteRepoPath));
    if (!AsyncFuture::waitForFinished(cloneFuture, 10000)) {
        throw std::runtime_error("Fixture setup failed: clone timed out");
    }
    if (cloneFuture.result().hasError()) {
        throw std::runtime_error(
            std::string("Fixture setup failed: clone error: ")
            + cloneFuture.result().errorMessage().toStdString());
    }

    fx.remoteRootData = std::make_unique<cwRootData>();
    fx.remoteRootData->account()->setName(config.remoteAccountName);
    fx.remoteRootData->account()->setEmail(config.remoteAccountEmail);

    fx.remoteProject = fx.remoteRootData->project();
    FIXTURE_REQUIRE(fx.remoteProject != nullptr);
    const QString clonedProjectPath = QDir(fx.clonePath).filePath(
        QFileInfo(fx.projectPath).fileName());
    FIXTURE_REQUIRE(QFileInfo::exists(clonedProjectPath));
    fx.remoteProject->loadFile(clonedProjectPath);
    fx.remoteProject->waitLoadToFinish();
    fx.remoteProject->waitSaveToFinish();

    fx.remoteRepository = fx.remoteProject->repository();
    FIXTURE_REQUIRE(fx.remoteRepository != nullptr);
    fx.remoteRepository->setAccount(fx.remoteRootData->account());

    return fx;
}
