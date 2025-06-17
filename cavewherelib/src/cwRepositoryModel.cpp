
//Our includes
#include "cwRepositoryModel.h"
#include "cwRootData.h"

//Monad
#include "Monad/Monad.h"
#include "GitRepository.h"

//Qt includes
#include <QStandardPaths>

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

                   m_repositories.append(dir.value());
                   endInsertRows();
                   saveRepositories();

                   //Return success
                   return ResultBase();
               })
            .replaceIfErrorWith(LibGit2Error);
    });
}

void cwRepositoryModel::clear()
{
    beginResetModel();
    m_repositories.clear();
    saveRepositories();
    endResetModel();
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
            return QDir(QDir(info.absoluteFilePath()).absoluteFilePath(name));
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
    auto defaultPath = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    m_defaultRepositoryDir = settings.value(DefaultDirKey, defaultPath).toString();
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
