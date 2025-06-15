#pragma once

#include "Monad/Result.h"
#include <QAbstractListModel>
#include <QDir>
#include <QSettings>

class cwRepositoryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RepositoryRoles {
        PathRole = Qt::UserRole + 1,
        NameRole
    };

    explicit cwRepositoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE Monad::ResultBase addRepository(const QDir& repositoryDir);
    Q_INVOKABLE Monad::ResultBase addRepository(const QUrl& localDir, const QString& name);

private:
    void loadRepositories();
    void saveRepositories() const;

    QList<QDir> m_repositories;

    static constexpr char SettingsKey[] = "repositories";
};

