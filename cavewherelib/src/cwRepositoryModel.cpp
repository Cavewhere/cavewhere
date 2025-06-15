#include "cwRepositoryModel.h"
#include "Monad/Monad.h"
#include "GitRepository.h"


cwRepositoryModel::cwRepositoryModel(QObject* parent)
    : QAbstractListModel(parent)
{
    loadRepositories();
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

Monad::ResultBase cwRepositoryModel::addRepository(const QDir& repositoryDir)
{
    return Monad::mtry([&](){
        const int newIndex = m_repositories.count();
        beginInsertRows(QModelIndex(), newIndex, newIndex);

        QQuickGit::GitRepository repo;
        repo.setDirectory(repositoryDir);
        repo.initRepository();

        m_repositories.append(repositoryDir);
        endInsertRows();
        saveRepositories();

        //Return success
        return Monad::ResultBase();
    });
}

Monad::ResultBase cwRepositoryModel::addRepository(const QUrl &localDir, const QString &name)
{
    auto quotedFilename = [localDir]() {
        return QStringLiteral("\"") + localDir.toString() + QStringLiteral("\"");
    };

    if(localDir.isLocalFile()) {
        QFileInfo info(localDir.toLocalFile());
        if(info.isDir()) {
            return addRepository(QDir(info.absoluteFilePath()).absoluteFilePath(name));
        } else {
            if(info.exists()) {
                return Monad::ResultBase(quotedFilename() + QStringLiteral(" doesn't exist"));
            } else {
                return Monad::ResultBase(quotedFilename() + QStringLiteral(" is not a directory"));
            }
        }
    } else {
        return Monad::ResultBase(quotedFilename() + QStringLiteral("is not a non-local directory"));
    }
}


void cwRepositoryModel::loadRepositories()
{
    QSettings settings;
    const QStringList list = settings.value(SettingsKey).toStringList();
    for (const QString& path : list) {
        QDir dir(path);
        if (dir.exists() && QQuickGit::GitRepository::isRepository(dir)) {
            m_repositories.append(dir);
        }
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
