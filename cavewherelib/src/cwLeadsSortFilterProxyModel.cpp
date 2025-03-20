/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwLeadsSortFilterProxyModel.h"
#include "cwLeadModel.h"

cwLeadsSortFilterProxyModel::cwLeadsSortFilterProxyModel(QObject *parent) : cwSortFilterProxyModel(parent)
{
    QSortFilterProxyModel::setSortRole(cwLeadModel::LeadNearestStation);
}

cwLeadsSortFilterProxyModel::~cwLeadsSortFilterProxyModel()
{

}

/**
 * @brief cwLeadsSortFilterProxyModel::lessThan
 * @param left
 * @param right
 * @return True if left is lessThan right
 *
 * This will cause this model sort by complete first, and then by other columns second
 */
bool cwLeadsSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    Q_ASSERT(qobject_cast<cwLeadModel*>(sourceModel()) != nullptr);
//    cwLeadModel* leadModel = static_cast<cwLeadModel*>(sourceModel());

    bool leftCompleted = left.data(cwLeadModel::LeadCompleted).toBool();
    bool rightCompleted = right.data(cwLeadModel::LeadCompleted).toBool();

    if(leftCompleted == rightCompleted) {
        return cwSortFilterProxyModel::lessThan(left, right);
    }

    return leftCompleted < rightCompleted;
}

