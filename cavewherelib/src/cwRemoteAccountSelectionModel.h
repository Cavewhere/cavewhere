#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QPointer>

class cwRemoteAccountModel;

class cwRemoteAccountSelectionModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteAccountSelectionModel)

public:
    enum EntryType {
        NoneEntry,
        AccountEntry,
        AddEntry,
        SeparatorEntry
    };
    Q_ENUM(EntryType)

    enum Roles {
        LabelRole = Qt::UserRole + 1,
        ProviderRole,
        UsernameRole,
        EntryTypeRole
    };

    Q_PROPERTY(cwRemoteAccountModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged)

    explicit cwRemoteAccountSelectionModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    cwRemoteAccountModel* sourceModel() const { return m_baseModel; }
    void setSourceModel(cwRemoteAccountModel* model);

signals:
    void entriesChanged();
    void sourceModelChanged();

private:

    bool showAccountSeparator() const;
    bool showAddSeparator() const;

    QPointer<cwRemoteAccountModel> m_baseModel;
};
