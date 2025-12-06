// Our includes
#include "cwSelectionProxyModel.h"

// Qt includes
#include <QHash>
#include <QVector>

cwSelectionProxyModel::cwSelectionProxyModel(QObject *parent) :
    QIdentityProxyModel(parent)
{
    recomputeSelectionRole();
}

int cwSelectionProxyModel::idRole() const
{
    return m_idRole.value();
}

void cwSelectionProxyModel::setIdRole(int idRole)
{
    if(m_idRole == idRole) {
        return;
    }

    m_idRole = idRole;
    clearSelectionInternal(true);
}

QBindable<int> cwSelectionProxyModel::bindableIdRole()
{
    return &m_idRole;
}

int cwSelectionProxyModel::selectionCount() const
{
    return m_selectedIds.size();
}

int cwSelectionProxyModel::selectionRole() const
{
    return m_selectionRole;
}

bool cwSelectionProxyModel::isSelected(const QModelIndex &index) const
{
    if(!index.isValid() || m_idRole < 0) {
        return false;
    }

    return m_selectedIds.contains(idForIndex(index));
}

bool cwSelectionProxyModel::setSelected(const QModelIndex &index, bool selected)
{
    if(!index.isValid() || m_idRole < 0) {
        return false;
    }

    const auto id = idForIndex(index);
    if(id.isEmpty()) {
        return false;
    }

    const bool alreadySelected = m_selectedIds.contains(id);

    if(selected && !alreadySelected) {
        m_selectedIds.insert(id);
    } else if(!selected && alreadySelected) {
        m_selectedIds.remove(id);
    } else {
        return true;
    }

    emit dataChanged(index, index, {m_selectionRole});
    emit selectionChanged();
    return true;
}

void cwSelectionProxyModel::toggleSelected(const QModelIndex &index)
{
    setSelected(index, !isSelected(index));
}

void cwSelectionProxyModel::selectAll()
{
    if(m_idRole < 0 || rowCount() == 0) {
        return;
    }

    bool changed = false;
    for(int row = 0; row < rowCount(); row++) {
        auto modelIndex = index(row, 0);
        const auto id = idForIndex(modelIndex);
        if(!id.isEmpty() && !m_selectedIds.contains(id)) {
            m_selectedIds.insert(id);
            changed = true;
        }
    }

    if(changed) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {m_selectionRole});
        emit selectionChanged();
    }
}

void cwSelectionProxyModel::clearSelection()
{
    clearSelectionInternal(true);
}

QVariant cwSelectionProxyModel::data(const QModelIndex &index, int role) const
{
    if(role == m_selectionRole) {
        return isSelected(index);
    }

    return QIdentityProxyModel::data(index, role);
}

bool cwSelectionProxyModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(role == m_selectionRole) {
        return setSelected(index, value.toBool());
    }

    return QIdentityProxyModel::setData(index, value, role);
}

QHash<int, QByteArray> cwSelectionProxyModel::roleNames() const
{
    auto roles = QIdentityProxyModel::roleNames();
    roles.insert(m_selectionRole, m_selectionRoleName);
    return roles;
}

void cwSelectionProxyModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    if(sourceModel == QIdentityProxyModel::sourceModel()) {
        return;
    }

    for(const auto& connection : std::as_const(m_sourceConnections)) {
        disconnect(connection);
    }
    m_sourceConnections.clear();

    clearSelectionInternal(false);
    QIdentityProxyModel::setSourceModel(sourceModel);
    recomputeSelectionRole();

    if(sourceModel) {
        m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved,
                                           this, &cwSelectionProxyModel::handleRowsAboutToBeRemoved));
        m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset,
                                           this, &cwSelectionProxyModel::handleModelAboutToBeReset));
    }
}

QString cwSelectionProxyModel::idForIndex(const QModelIndex &index) const
{
    return index.data(m_idRole).toString();
}

void cwSelectionProxyModel::clearSelectionInternal(bool emitDataChanged)
{
    if(m_selectedIds.isEmpty()) {
        return;
    }

    m_selectedIds.clear();

    if(emitDataChanged && rowCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {m_selectionRole});
    }

    emit selectionChanged();
}

void cwSelectionProxyModel::recomputeSelectionRole()
{
    auto roles = QIdentityProxyModel::roleNames();
    int candidate = Qt::UserRole;
    while(roles.contains(candidate)) {
        candidate++;
    }

    m_selectionRole = candidate;
}

void cwSelectionProxyModel::handleRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    bool changed = false;
    if(m_idRole >= 0) {
        for(int row = first; row <= last; row++) {
            auto modelIndex = index(row, 0, parent);
            auto id = idForIndex(modelIndex);
            changed = m_selectedIds.remove(id) > 0 || changed;
        }
    }

    if(changed) {
        emit selectionChanged();
    }
}

void cwSelectionProxyModel::handleModelAboutToBeReset()
{
    const bool hadSelection = !m_selectedIds.isEmpty();
    m_selectedIds.clear();
    if(hadSelection) {
        emit selectionChanged();
    }
}
