
//Our includes
#include "cwRecentProjectModel.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"

//Monad
#include "Monad/Monad.h"
#include "GitRepository.h"

//Qt includes
#include <QStandardPaths>
#include <QFileInfo>
#include <QUrl>

using namespace Monad;

cwRecentProjectModel::cwRecentProjectModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_defaultRepositoryDirNotifier = m_defaultRepositoryDir.addNotifier([this]() {
        QSettings settings;
        settings.setValue(DefaultDirKey, QUrl::fromLocalFile(QDir(m_defaultRepositoryDir.value().toLocalFile()).absolutePath()));
    });

    loadSettings();
}

int cwRecentProjectModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_repositories.count();
}

QVariant cwRecentProjectModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_repositories.count()) {
        return {};
    }

    const RepositoryEntry& entry = m_repositories.at(index.row());
    const QFileInfo info(entry.projectPath);
    switch (role) {
    case PathRole:
        return info.absoluteFilePath();
    case NameRole:
        return info.isDir() ? info.fileName() : info.fileName();
    default:
        return {};
    }
}

QHash<int, QByteArray> cwRecentProjectModel::roleNames() const
{
    return QHash<int, QByteArray>{
        { PathRole, "pathRole" },
        { NameRole, "nameRole" }
    };
}

Monad::ResultBase cwRecentProjectModel::addRepository(const cwResultDir& dir)
{
    return dir.then([this](const Result<QDir>& dir)->ResultBase {
        return Monad::mtry([&](){
                   const int newIndex = m_repositories.count();
                   beginInsertRows(QModelIndex(), newIndex, newIndex);

                   cwSaveLoad::initializeGitRepository(dir.value());

                   m_repositories.append({dir.value().absolutePath()});
                   endInsertRows();
                   saveRepositories();

                   //Return success
                   return ResultBase();
               })
            .replaceIfErrorWith(LibGit2Error);
    });
}

Monad::ResultBase cwRecentProjectModel::addRepositoryDirectory(const QDir& dir)
{
    if (!dir.exists()) {
        return Monad::ResultBase(QStringLiteral("Repository path %1 does not exist").arg(dir.absolutePath()), PathError);
    }

    if (!QQuickGit::GitRepository::isRepository(dir)) {
        return Monad::ResultBase(QStringLiteral("Repository path %1 is not a git repository").arg(dir.absolutePath()), PathError);
    }

    const QString repoPath = dir.absolutePath();
    for (const RepositoryEntry& repo : m_repositories) {
        if (QFileInfo(repo.projectPath).absoluteFilePath() == repoPath) {
            return Monad::ResultBase();
        }
    }

    const int newIndex = m_repositories.count();
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_repositories.append({repoPath});
    endInsertRows();
    saveRepositories();

    return Monad::ResultBase();
}

Monad::ResultString cwRecentProjectModel::repositoryProjectFile(int index) const
{
    if (index < 0 || index >= m_repositories.count()) {
        return Monad::ResultString(QStringLiteral("Repository index %1 is out of range").arg(index), PathError);
    }

    const QString storedPath = QFileInfo(m_repositories.at(index).projectPath).absoluteFilePath();
    const QFileInfo entryInfo(storedPath);
    if (!entryInfo.exists()) {
        return Monad::ResultString(QStringLiteral("Repository path %1 does not exist").arg(storedPath), PathError);
    }

    if (entryInfo.isFile()) {
        const QString suffix = entryInfo.suffix();
        if (suffix.compare(QStringLiteral("cwproj"), Qt::CaseInsensitive) == 0
            || suffix.compare(QStringLiteral("cw"), Qt::CaseInsensitive) == 0) {
            return Monad::ResultString(entryInfo.absoluteFilePath());
        }

        return Monad::ResultString(QStringLiteral("Project file %1 is not a CaveWhere project").arg(entryInfo.fileName()), PathError);
    }

    const QDir dir(entryInfo.absoluteFilePath());
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

Monad::ResultBase cwRecentProjectModel::addRepositoryFromProjectFile(const QUrl& projectFileUrl)
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

    const QString storedPath = fileInfo.absoluteFilePath();
    for (const RepositoryEntry& repo : m_repositories) {
        if (QFileInfo(repo.projectPath).absoluteFilePath() == storedPath) {
            return Monad::ResultBase();
        }
    }

    const int newIndex = m_repositories.count();
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_repositories.append({storedPath});
    endInsertRows();
    saveRepositories();

    return Monad::ResultBase();
}

void cwRecentProjectModel::clear()
{
    beginResetModel();
    m_repositories.clear();
    saveRepositories();
    endResetModel();
}


void cwRecentProjectModel::setProject(cwProject* project)
{
    Q_UNUSED(project);
}

void cwRecentProjectModel::loadSettings()
{
    QSettings settings;

    //Load repositories list
    const QStringList list = settings.value(SettingsKey).toStringList();
    bool pruned = false;
    for (const QString& path : list) {
        const QFileInfo info(path);
        if (!info.exists()) {
            pruned = true;
            continue;
        }

        if (info.isDir()) {
            const QDir dir(info.absoluteFilePath());
            if (QQuickGit::GitRepository::isRepository(dir)) {
                m_repositories.append({dir.absolutePath()});
            } else {
                pruned = true;
            }
            continue;
        }

        const QString suffix = info.suffix();
        if (suffix.compare(QStringLiteral("cwproj"), Qt::CaseInsensitive) == 0
            || suffix.compare(QStringLiteral("cw"), Qt::CaseInsensitive) == 0) {
            m_repositories.append({info.absoluteFilePath()});
        } else {
            pruned = true;
        }
    }

    if (pruned) {
        saveRepositories();
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

void cwRecentProjectModel::saveRepositories() const
{
    QSettings settings;
    QStringList list;
    for (const RepositoryEntry& entry : m_repositories) {
        list.append(QFileInfo(entry.projectPath).absoluteFilePath());
    }
    settings.setValue(SettingsKey, list);
}
