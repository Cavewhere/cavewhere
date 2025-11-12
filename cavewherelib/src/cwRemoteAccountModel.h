#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

class cwRemoteAccountModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteAccountModel)

public:
    enum class Provider {
        Unknown,
        GitHub,
        Bitbucket,
        GitLab
    };
    Q_ENUM(Provider)

    enum Roles {
        ProviderRole = Qt::UserRole + 1,
        UsernameRole,
        LabelRole
    };

    explicit cwRemoteAccountModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addOrUpdateAccount(Provider provider, const QString& username);
    Q_INVOKABLE void removeAccount(Provider provider, const QString& username);
    Q_INVOKABLE void clear();

    static QString providerToString(Provider provider);
    static Provider providerFromString(const QString& str);
    static QString providerDisplayName(Provider provider);

signals:
    void accountsChanged();

private:
    struct Entry {
        Provider provider = Provider::Unknown;
        QString username;
    };

    int findAccount(Provider provider, const QString& username) const;
    void loadFromSettings();
    void saveToSettings() const;

    QList<Entry> m_entries;
};
