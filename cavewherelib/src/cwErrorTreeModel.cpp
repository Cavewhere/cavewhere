/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwErrorTreeModel.h"
#include "cwErrorModel.h"

cwErrorTreeModel::cwErrorTreeModel(QObject* parent) :
    QSortFilterProxyModel(parent)
{
}

void cwErrorTreeModel::setRoot(cwErrorModel* root)
{
    if (m_root == root) {
        return;
    }
    m_root = root;
    emit rootChanged();
}

void cwErrorTreeModel::setSuppressedOnly(bool suppressedOnly)
{
    if (m_suppressedOnly == suppressedOnly) {
        return;
    }
    m_suppressedOnly = suppressedOnly;
    emit suppressedOnlyChanged();
}

void cwErrorTreeModel::setKindFilter(const QString& kindFilter)
{
    if (m_kindFilter == kindFilter) {
        return;
    }
    m_kindFilter = kindFilter;
    emit kindFilterChanged();
}

QStringList cwErrorTreeModel::kinds() const
{
    return {};
}

int cwErrorTreeModel::count() const
{
    return 0;
}

QVariant cwErrorTreeModel::data(const QModelIndex& index, int role) const
{
    return QSortFilterProxyModel::data(index, role);
}

QHash<int, QByteArray> cwErrorTreeModel::roleNames() const
{
    auto names = QSortFilterProxyModel::roleNames();
    names.insert(static_cast<int>(Roles::KindRole), QByteArrayLiteral("kind"));
    names.insert(static_cast<int>(Roles::ErrorModelRole), QByteArrayLiteral("errorModel"));
    return names;
}
