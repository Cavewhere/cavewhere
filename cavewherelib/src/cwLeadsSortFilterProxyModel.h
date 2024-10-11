/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEADSSORTFILTERPROXYMODEL_H
#define CWLEADSSORTFILTERPROXYMODEL_H

//Our include
#include "cwSortFilterProxyModel.h"

/**
 * @brief The cwLeadsSortFilterProxyModel class
 *
 * This is used sort LeadModel. This will sort the LeadsModel by completed and then by
 * any of the sortRoles.
 */
class cwLeadsSortFilterProxyModel : public cwSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit cwLeadsSortFilterProxyModel(QObject *parent = 0);
    ~cwLeadsSortFilterProxyModel();

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

signals:

public slots:
};

#endif // CWLEADSSORTFILTERPROXYMODEL_H
