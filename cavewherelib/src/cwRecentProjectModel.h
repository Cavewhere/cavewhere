#pragma once

//Monad
#include "Monad/Result.h"

//Qt
#include <QAbstractListModel>
#include <QFileInfo>
#include <QPropertyBinding>
#include <QQmlEngine>
#include <QSettings>
#include <QUrl>

//Our
#include "cwResultDir.h"
#include "CaveWhereLibExport.h"

class cwProject;

class CAVEWHERE_LIB_EXPORT cwRecentProjectModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RecentProjectModel);

    Q_PROPERTY(QUrl defaultRepositoryDir READ defaultRepositoryDir WRITE setDefaultRepositoryDir NOTIFY defaultRepositoryDirChanged BINDABLE bindableDefaultRepositoryDir)

public:
    enum RepositoryRoles {
        PathRole = Qt::UserRole + 1,
        NameRole
    };
    Q_ENUM(RepositoryRoles)

    enum ErrorCode {
        NameError = Monad::ResultBase::CustomError + 1,
        PathError,
        LibGit2Error
    };

    Q_ENUM(ErrorCode)

    explicit cwRecentProjectModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QUrl defaultRepositoryDir() const { return m_defaultRepositoryDir.value(); }
    void setDefaultRepositoryDir(const QUrl& defaultRepositoryDir) { m_defaultRepositoryDir = defaultRepositoryDir; }
    QBindable<QUrl> bindableDefaultRepositoryDir() { return &m_defaultRepositoryDir; }

    Q_INVOKABLE Monad::ResultBase addRepository(const cwResultDir& dir);
    Q_INVOKABLE Monad::ResultBase addRepositoryDirectory(const QDir& dir);
    Q_INVOKABLE Monad::ResultString repositoryProjectFile(int index) const;
    Q_INVOKABLE Monad::ResultBase addRepositoryFromProjectFile(const QUrl& projectFileUrl);

    Q_INVOKABLE void clear();
    void setProject(cwProject* project);

signals:
    void defaultRepositoryDirChanged();

private:
    struct RepositoryEntry {
        QString projectPath;
    };

    void loadSettings();
    void saveRepositories() const;

    QList<RepositoryEntry> m_repositories;

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwRecentProjectModel, QUrl, m_defaultRepositoryDir, QUrl(), &cwRecentProjectModel::defaultRepositoryDirChanged);
    QPropertyNotifier m_defaultRepositoryDirNotifier;

    static constexpr char SettingsKey[] = "repositories";
    static constexpr char DefaultDirKey[] = "defaultRepositoryDir";
};
