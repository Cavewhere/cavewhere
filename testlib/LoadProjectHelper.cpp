#include "LoadProjectHelper.h"

//Qt includes
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
#include "cwFutureManagerModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwErrorListModel.h"
#include "GitRepository.h"
#include "Account.h"
#include "cwRemoteCredentialStore.h"
#include "asyncfuture.h"
#include "LfsServer.h"

//libgit2
#include "git2.h"
#include <qtkeychain/keychain.h>
#include <QSignalSpy>

//Std includes
#include <exception>
#include <memory>

namespace {
std::unique_ptr<LfsServer> g_syncLfsServer;

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

    QTemporaryDir tempDir;
    tempDir.setAutoRemove(false);

    QFileInfo info(filename);
    QString newFileLocation = tempDir.path() + "/" + info.fileName();

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

bool TestHelper::fileExists(const QUrl &filename) const
{
    QFileInfo info(filename.toLocalFile());
    return info.exists();
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

int TestHelper::projectModifiedFileCount(cwProject* project) const
{
    if (project == nullptr || project->repository() == nullptr) {
        return -1;
    }

    project->repository()->checkStatus();
    return project->repository()->modifiedFileCount();
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
    git_repository* bareRepo = nullptr;
    if (git_repository_init(&bareRepo, remoteRepoPath.toLocal8Bit().constData(), 1) != GIT_OK) {
        const git_error* error = git_error_last();
        result.errorMessage = error ? QString::fromUtf8(error->message)
                                    : QStringLiteral("Failed to initialize bare repository.");
        return result;
    }
    if (bareRepo) {
        git_repository_free(bareRepo);
    }

    QQuickGit::Account account;
    account.setName(QStringLiteral("QML Sync Fixture"));
    account.setEmail(QStringLiteral("qml.sync.fixture@example.com"));

    cwProject project;
    addTokenManager(&project);
    project.setGitAccount(&account);
    const QString datasetFile = copyToTempFolder(QStringLiteral("://datasets/test_cwProject/Phake Cave 3000.cw"));
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
    git_repository* bareRepo = nullptr;
    if (git_repository_init(&bareRepo, remoteRepoPath.toLocal8Bit().constData(), 1) != GIT_OK) {
        const git_error* error = git_error_last();
        result.errorMessage = error ? QString::fromUtf8(error->message)
                                    : QStringLiteral("Failed to initialize bare repository.");
        return result;
    }
    if (bareRepo) {
        git_repository_free(bareRepo);
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
    const QString key = cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitHub,
                                                                 accountId);
    if (key.isEmpty()) {
        return false;
    }

    QKeychain::WritePasswordJob job(QStringLiteral("CaveWhere"));
    job.setAutoDelete(false);
    job.setKey(key);
    job.setTextData(token);
    QSignalSpy spy(&job, &QKeychain::Job::finished);
    job.start();
    if (!spy.wait(5000)) {
        return false;
    }

    return job.error() == QKeychain::NoError;
}

bool TestHelper::clearGitHubAccessTokenForAccount(const QString& accountId) const
{
    const QString key = cwRemoteCredentialStore::accessTokenKey(cwRemoteAccountModel::Provider::GitHub,
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
