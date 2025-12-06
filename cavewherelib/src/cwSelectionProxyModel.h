#ifndef CWSELECTIONPROXYMODEL_H
#define CWSELECTIONPROXYMODEL_H

// Qt includes
#include <QIdentityProxyModel>
#include <QPropertyBinding>
#include <QQmlEngine>
#include <QSet>

// Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwSelectionProxyModel : public QIdentityProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SelectionProxyModel)

    Q_PROPERTY(int idRole READ idRole WRITE setIdRole NOTIFY idRoleChanged BINDABLE bindableIdRole)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectionRole READ selectionRole CONSTANT)

public:
    explicit cwSelectionProxyModel(QObject* parent = nullptr);

    int idRole() const;
    void setIdRole(int idRole);
    QBindable<int> bindableIdRole();

    int selectionCount() const;
    int selectionRole() const;

    Q_INVOKABLE bool isSelected(const QModelIndex& index) const;
    Q_INVOKABLE bool setSelected(const QModelIndex& index, bool selected);
    Q_INVOKABLE void toggleSelected(const QModelIndex& index);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();

    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    void setSourceModel(QAbstractItemModel* sourceModel) override;

signals:
    void idRoleChanged();
    void selectionChanged();

private:
    QByteArray m_selectionRoleName = QByteArrayLiteral("selectedRole");
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwSelectionProxyModel, int, m_idRole, -1, &cwSelectionProxyModel::idRoleChanged);
    int m_selectionRole = Qt::UserRole;
    QSet<QString> m_selectedIds;
    QVector<QMetaObject::Connection> m_sourceConnections;

    QString idForIndex(const QModelIndex& index) const;
    void clearSelectionInternal(bool emitDataChanged);
    void recomputeSelectionRole();
    void handleRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void handleModelAboutToBeReset();
};

#endif // CWSELECTIONPROXYMODEL_H
