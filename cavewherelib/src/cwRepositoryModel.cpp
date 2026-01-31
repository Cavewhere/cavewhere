
//Our includes
#include "cwRepositoryModel.h"
#include "cwRootData.h"
#include "cwProject.h"
#include "cwSaveLoad.h"

//Monad
#include "Monad/Monad.h"
#include "GitRepository.h"

//Qt includes
#include <QStandardPaths>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>

namespace cw::git {
void ensureGitExcludeHasCacheEntry(const QDir& repoDir);
}

using namespace Monad;

cwRepositoryModel::cwRepositoryModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_defaultRepositoryDirNotifier = m_defaultRepositoryDir.addNotifier([this]() {
        QSettings settings;
        settings.setValue(DefaultDirKey, QUrl::fromLocalFile(QDir(m_defaultRepositoryDir.value().toLocalFile()).absolutePath()));
    });

    loadSettings();
}

int cwRepositoryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_repositories.count();
}

QVariant cwRepositoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_repositories.count()) {
        return {};
    }

    const QDir& dir = m_repositories.at(index.row());
    switch (role) {
    case PathRole:
        return dir.absolutePath();
    case NameRole:
        return dir.dirName();
    default:
        return {};
    }
}

QHash<int, QByteArray> cwRepositoryModel::roleNames() const
{
    return QHash<int, QByteArray>{
        { PathRole, "pathRole" },
        { NameRole, "nameRole" }
    };
}

Monad::ResultBase cwRepositoryModel::addRepository(const cwResultDir& dir)
{
    return dir.then([this](const Result<QDir>& dir)->ResultBase {
        return Monad::mtry([&](){
                   const int newIndex = m_repositories.count();
                   beginInsertRows(QModelIndex(), newIndex, newIndex);

                   QQuickGit::GitRepository repo;
                   repo.setDirectory(dir.value());
                   repo.initRepository();

                   qWarning() << "TODO: Repositories should be init'ed through cwSaveLoad" << LOCATION;
                   cwSaveLoad::ensureGitExcludeHasCacheEntry(dir.value());


                   m_repositories.append(dir.value());
                   endInsertRows();
                   saveRepositories();

                   //Return success
                   return ResultBase();
               })
            .replaceIfErrorWith(LibGit2Error);
    });
}

Monad::ResultBase cwRepositoryModel::addRepositoryDirectory(const QDir& dir)
{
    if (!dir.exists()) {
        return Monad::ResultBase(QStringLiteral("Repository path %1 does not exist").arg(dir.absolutePath()), PathError);
    }

    if (!QQuickGit::GitRepository::isRepository(dir)) {
        return Monad::ResultBase(QStringLiteral("Repository path %1 is not a git repository").arg(dir.absolutePath()), PathError);
    }

    for (const QDir& repo : m_repositories) {
        if (repo.absolutePath() == dir.absolutePath()) {
            return Monad::ResultBase();
        }
    }

    const int newIndex = m_repositories.count();
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_repositories.append(dir);
    endInsertRows();
    saveRepositories();

    return Monad::ResultBase();
}

Monad::ResultString cwRepositoryModel::repositoryProjectFile(int index) const
{
    if (index < 0 || index >= m_repositories.count()) {
        return Monad::ResultString(QStringLiteral("Repository index %1 is out of range").arg(index), PathError);
    }

    const QDir dir = m_repositories.at(index);
    if (!dir.exists()) {
        return Monad::ResultString(QStringLiteral("Repository path %1 does not exist").arg(dir.absolutePath()), PathError);
    }

    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    QString projectFilePath;
    QString legacyProjectFilePath;

    for (const QFileInfo& entry : entries) {
        const QString suffix = entry.suffix();
        if (suffix.compare(QStringLiteral("cwproj"), Qt::CaseInsensitive) == 0) {
            if (!projectFilePath.isEmpty()) {
                return Monad::ResultString(QStringLiteral("Multiple CaveWhere project files found in %1").arg(dir.absolutePath()), PathError);
            }
            projectFilePath = entry.absoluteFilePath();
        } else if (suffix.compare(QStringLiteral("cw"), Qt::CaseInsensitive) == 0) {
            if (!legacyProjectFilePath.isEmpty()) {
                return Monad::ResultString(QStringLiteral("Multiple CaveWhere project files found in %1").arg(dir.absolutePath()), PathError);
            }
            legacyProjectFilePath = entry.absoluteFilePath();
        }
    }

    if (projectFilePath.isEmpty()) {
        projectFilePath = legacyProjectFilePath;
    }

    if (projectFilePath.isEmpty()) {
        return Monad::ResultString(QStringLiteral("No CaveWhere project file found in %1").arg(dir.absolutePath()), PathError);
    }

    return Monad::ResultString(projectFilePath);
}

Monad::ResultBase cwRepositoryModel::openRepository(int index, cwProject* project) const
{
    if (project == nullptr) {
        return Monad::ResultBase(QStringLiteral("Project instance is null"), PathError);
    }

    const Monad::ResultString fileResult = repositoryProjectFile(index);
    if (fileResult.hasError()) {
        return Monad::ResultBase(fileResult.errorMessage(), fileResult.errorCode());
    }

    project->loadFile(QUrl::fromLocalFile(fileResult.value()).toString());
    return Monad::ResultBase();
}

Monad::ResultBase cwRepositoryModel::addRepositoryFromProjectFile(const QUrl& projectFileUrl)
{
    if (!projectFileUrl.isValid()) {
        return Monad::ResultBase(QStringLiteral("Project file URL is invalid"), PathError);
    }

    if (!projectFileUrl.isLocalFile()) {
        return Monad::ResultBase(QStringLiteral("Project file must be a local file"), PathError);
    }

    const QFileInfo fileInfo(projectFileUrl.toLocalFile());
    if (!fileInfo.exists()) {
        return Monad::ResultBase(QStringLiteral("Project file %1 does not exist").arg(fileInfo.absoluteFilePath()), PathError);
    }

    const QString suffix = fileInfo.suffix();
    if (suffix.compare(QStringLiteral("cwproj"), Qt::CaseInsensitive) != 0
        && suffix.compare(QStringLiteral("cw"), Qt::CaseInsensitive) != 0) {
        return Monad::ResultBase(QStringLiteral("Project file %1 is not a CaveWhere project").arg(fileInfo.fileName()), PathError);
    }

    const QString repoPath = fileInfo.absoluteDir().absolutePath();
    for (const QDir& repo : m_repositories) {
        if (repo.absolutePath() == repoPath) {
            return Monad::ResultBase();
        }
    }

    return addRepository(QDir(repoPath));
}

void cwRepositoryModel::clear()
{
    beginResetModel();
    m_repositories.clear();
    saveRepositories();
    endResetModel();
}

void cwRepositoryModel::setProject(cwProject* project)
{
    if (m_project == project) {
        return;
    }

    clearProjectConnections();
    m_project = project;

    if (!m_project) {
        return;
    }

    m_projectConnections.append(QObject::connect(m_project, &cwProject::filenameChanged, this, [this]() {
        handleProjectStateChanged();
    }));

    m_projectConnections.append(QObject::connect(m_project, &cwProject::isTemporaryProjectChanged, this, [this]() {
        handleProjectStateChanged();
    }));

    handleProjectStateChanged();
}

cwResultDir cwRepositoryModel::repositoryDir(const QUrl &localDir, const QString &name) const
{
    if(name.isEmpty()) {
        return cwResultDir(QStringLiteral("Caving area name is empty"), NameError);
    }

    auto quotedFilename = [](const QString& fullName, const QString& name) {
        auto quote = [](const QString& str){
            return  QStringLiteral("\"") + str + QStringLiteral("\" ");
        };

        if(!cwRootData::isMobileBuild()) {
            //Desktop build
            return quote(fullName);
        } else {
            return quote(name);
        }
    };

    if(localDir.isLocalFile()) {
        QFileInfo info(localDir.toLocalFile() + QStringLiteral("/") + name);
        if(info.exists()) {
            return cwResultDir(QString(quotedFilename(info.absoluteFilePath(), name) + QStringLiteral("exists, use a different name")), PathError);
        } else {
            //Success
            return QDir(info.absoluteFilePath());
        }
    } else {
        //Is a url and not a local file
        return cwResultDir(quotedFilename(localDir.toString(), name) + QStringLiteral("is not a non-local directory"), PathError);
    }
}

void cwRepositoryModel::loadSettings()
{
    QSettings settings;

    //Load repositories list
    const QStringList list = settings.value(SettingsKey).toStringList();
    for (const QString& path : list) {
        QDir dir(path);
        if (dir.exists() && QQuickGit::GitRepository::isRepository(dir)) {
            m_repositories.append(dir);
        }
    }

    //Load default directory
    QUrl defaultPath;
    if (cwRootData::isMobileBuild()) {
        const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir baseDir(basePath);
        baseDir.mkpath(QStringLiteral("."));
        QDir repoDir(baseDir.filePath(QStringLiteral("Repositories")));
        repoDir.mkpath(QStringLiteral("."));
        defaultPath = QUrl::fromLocalFile(repoDir.absolutePath());
    } else {
        defaultPath = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    }

    const QUrl storedPath = settings.value(DefaultDirKey, defaultPath).toUrl();
    if (cwRootData::isMobileBuild()) {
        m_defaultRepositoryDir = defaultPath;
    } else {
        m_defaultRepositoryDir = storedPath.isValid() ? storedPath : defaultPath;
    }
}

void cwRepositoryModel::saveRepositories() const
{
    QSettings settings;
    QStringList list;
    for (const QDir& dir : m_repositories) {
        list.append(dir.absolutePath());
    }
    settings.setValue(SettingsKey, list);
}

void cwRepositoryModel::handleProjectStateChanged()
{
    if (!m_project) {
        return;
    }

    if (m_project->isTemporaryProject()) {
        return;
    }

    const QString filename = m_project->filename();
    if (filename.isEmpty()) {
        return;
    }

    const auto result = addRepositoryFromProjectFile(QUrl::fromLocalFile(filename));
    if (result.hasError()) {
        qWarning() << "Failed to add repository for project" << filename << ":" << result.errorMessage();
    }
}

void cwRepositoryModel::clearProjectConnections()
{
    for (const auto& connection : m_projectConnections) {
        QObject::disconnect(connection);
    }
    m_projectConnections.clear();
}
