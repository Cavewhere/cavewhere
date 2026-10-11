/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeGalleryFilterModel.h"

cwPassageTubeGalleryFilterModel::cwPassageTubeGalleryFilterModel(QObject* parent) :
    QSortFilterProxyModel(parent)
{
    sort(0);
}

QString cwPassageTubeGalleryFilterModel::filterText() const
{
    return m_filterText;
}

void cwPassageTubeGalleryFilterModel::setFilterText(const QString& filterText)
{
    if (m_filterText == filterText) {
        return;
    }
    beginFilterChange();
    m_filterText = filterText;
    endFilterChange(Direction::Rows);
    emit filterTextChanged();
}

bool cwPassageTubeGalleryFilterModel::failingFirst() const
{
    return m_failingFirst;
}

void cwPassageTubeGalleryFilterModel::setFailingFirst(bool failingFirst)
{
    if (m_failingFirst == failingFirst) {
        return;
    }
    m_failingFirst = failingFirst;
    invalidate();
    emit failingFirstChanged();
}

int cwPassageTubeGalleryFilterModel::sourceRow(int proxyRow) const
{
    return mapToSource(index(proxyRow, 0)).row();
}

bool cwPassageTubeGalleryFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_filterText.isEmpty()) {
        return true;
    }
    const QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto matches = [&](int role) {
        return row.data(role).toString().contains(m_filterText, Qt::CaseInsensitive);
    };
    return matches(cwPassageTubeGalleryModel::NameRole) || matches(cwPassageTubeGalleryModel::TagsRole);
}

bool cwPassageTubeGalleryFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (m_failingFirst) {
        const bool leftFails = left.data(cwPassageTubeGalleryModel::IssueCountRole).toInt() > 0;
        const bool rightFails = right.data(cwPassageTubeGalleryModel::IssueCountRole).toInt() > 0;
        if (leftFails != rightFails) {
            return leftFails;
        }
    }
    return left.row() < right.row();
}
