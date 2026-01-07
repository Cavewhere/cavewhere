#pragma once

//Monad
#include "Monad/Result.h"

//Qt
#include <QAbstractListModel>
#include <QDir>
#include <QUrl>
#include <QSettings>
#include <QPropertyBinding>
#include <QQmlEngine>
#include <QPointer>
#include <QMetaObject>

//Our
#include "cwResultDir.h"

class cwProject;

class cwRepositoryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RepositoryModel);

    Q_PROPERTY(QUrl defaultRepositoryDir READ defaultRepositoryDir WRITE setDefaultRepositoryDir NOTIFY defaultRepositoryDirChanged BINDABLE bindableDefaultRepositoryDir)

public:
    enum RepositoryRoles {
        PathRole = Qt::UserRole + 1,
        NameRole
    };

    enum ErrorCode {
        NameError = Monad::ResultBase::CustomError + 1,
        PathError,
        LibGit2Error
    };

    Q_ENUM(ErrorCode)

    explicit cwRepositoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QUrl defaultRepositoryDir() const { return m_defaultRepositoryDir.value(); }
    void setDefaultRepositoryDir(const QUrl& defaultRepositoryDir) { m_defaultRepositoryDir = defaultRepositoryDir; }
    QBindable<QUrl> bindableDefaultRepositoryDir() { return &m_defaultRepositoryDir; }

    Q_INVOKABLE cwResultDir repositoryDir(const QUrl &localDir, const QString &name) const;
    Q_INVOKABLE Monad::ResultBase addRepository(const cwResultDir& dir);
    Q_INVOKABLE Monad::ResultBase addRepositoryDirectory(const QDir& dir);
    Q_INVOKABLE Monad::ResultString repositoryProjectFile(int index) const;
    Q_INVOKABLE Monad::ResultBase openRepository(int index, cwProject* project) const;
    Q_INVOKABLE Monad::ResultBase addRepositoryFromProjectFile(const QUrl& projectFileUrl);

    Q_INVOKABLE void clear();
    void setProject(cwProject* project);

signals:
    void defaultRepositoryDirChanged();

private:
    void loadSettings();
    void saveRepositories() const;
    void handleProjectStateChanged();
    void clearProjectConnections();

    QList<QDir> m_repositories;
    QPointer<cwProject> m_project;
    QList<QMetaObject::Connection> m_projectConnections;

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwRepositoryModel, QUrl, m_defaultRepositoryDir, QUrl(), &cwRepositoryModel::defaultRepositoryDirChanged);
    QPropertyNotifier m_defaultRepositoryDirNotifier;

    static constexpr char SettingsKey[] = "repositories";
    static constexpr char DefaultDirKey[] = "defaultRepositoryDir";
};
