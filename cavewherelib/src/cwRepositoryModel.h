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

//Our
#include "cwResultDir.h"

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

    Q_INVOKABLE void clear();

signals:
    void defaultRepositoryDirChanged();

private:
    void loadSettings();
    void saveRepositories() const;

    QList<QDir> m_repositories;

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwRepositoryModel, QUrl, m_defaultRepositoryDir, QUrl(), &cwRepositoryModel::defaultRepositoryDirChanged);
    QPropertyNotifier m_defaultRepositoryDirNotifier;

    static constexpr char SettingsKey[] = "repositories";
    static constexpr char DefaultDirKey[] = "defaultRepositoryDir";
};

