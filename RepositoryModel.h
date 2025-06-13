#pragma once

#include "Monad/Result.h"
#include <QAbstractListModel>
#include <QDir>
#include <QSettings>

namespace cwSketch {

class RepositoryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RepositoryRoles {
        PathRole = Qt::UserRole + 1,
        NameRole
    };

    explicit RepositoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE Monad::ResultBase addRepository(const QDir& repositoryDir);

private:
    void loadRepositories();
    void saveRepositories() const;

    QList<QDir> m_repositories;

    static constexpr char SettingsKey[] = "repositories";
};

};
