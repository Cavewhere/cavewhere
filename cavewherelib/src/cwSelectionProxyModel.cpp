// Our includes
#include "cwSelectionProxyModel.h"

// Qt includes
#include <QHash>
#include <QVector>

// Std includes
#include <algorithm>

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
    rebuildIdCache();
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
    rebuildIdCache();

    if(sourceModel) {
        m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved,
                                           this, &cwSelectionProxyModel::handleRowsAboutToBeRemoved));
        m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset,
                                           this, &cwSelectionProxyModel::handleModelAboutToBeReset));
        m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::dataChanged,
                                           this, &cwSelectionProxyModel::handleSourceDataChanged));
        m_sourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsInserted,
                                           this, [this](const QModelIndex& parent, int first, int last) {
            if(parent.isValid() || m_idRole < 0) {
                return;
            }
            if(m_cachedIds.size() != rowCount()) {
                rebuildIdCache();
                return;
            }
            for(int row = first; row <= last; ++row) {
                m_cachedIds.insert(row, idForIndex(index(row, 0)));
            }
        }));
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

void cwSelectionProxyModel::rebuildIdCache()
{
    m_cachedIds.clear();
    if(m_idRole < 0) {
        return;
    }

    m_cachedIds.reserve(rowCount());
    for(int row = 0; row < rowCount(); ++row) {
        m_cachedIds.append(idForIndex(index(row, 0)));
    }
}

void cwSelectionProxyModel::handleSourceDataChanged(const QModelIndex& topLeft,
                                                    const QModelIndex& bottomRight,
                                                    const QVector<int>& roles)
{
    if(m_idRole < 0) {
        return;
    }

    if(!roles.isEmpty() && !roles.contains(m_idRole)) {
        return;
    }

    if(topLeft.parent().isValid() || bottomRight.parent().isValid()) {
        rebuildIdCache();
        return;
    }

    if(m_cachedIds.size() != rowCount()) {
        rebuildIdCache();
        return;
    }

    QHash<QString, int> idCounts;
    for(const auto& id : std::as_const(m_cachedIds)) {
        if(!id.isEmpty()) {
            idCounts[id] += 1;
        }
    }

    bool selectionChanged = false;
    const int startRow = std::max(0, topLeft.row());
    const int endRow = std::min(rowCount() - 1, bottomRight.row());
    for(int row = startRow; row <= endRow; ++row) {
        const QString oldId = m_cachedIds.at(row);
        const QString newId = idForIndex(index(row, 0));
        if(oldId == newId) {
            continue;
        }

        if(!oldId.isEmpty()) {
            auto countIter = idCounts.find(oldId);
            if(countIter != idCounts.end()) {
                countIter.value() -= 1;
                if(countIter.value() <= 0 && m_selectedIds.remove(oldId) > 0) {
                    selectionChanged = true;
                }
            }
        }

        if(!newId.isEmpty()) {
            idCounts[newId] += 1;
        }

        m_cachedIds[row] = newId;
    }

    if(selectionChanged) {
        if(rowCount() > 0) {
            emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {m_selectionRole});
        }
        emit this->selectionChanged();
    }
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

    if(!parent.isValid() && !m_cachedIds.isEmpty()) {
        const int removeCount = last - first + 1;
        if(first >= 0 && removeCount > 0 && first + removeCount <= m_cachedIds.size()) {
            m_cachedIds.remove(first, removeCount);
        } else {
            rebuildIdCache();
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
    m_cachedIds.clear();
    if(hadSelection) {
        emit selectionChanged();
    }
}
