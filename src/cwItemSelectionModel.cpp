/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwItemSelectionModel.h"

cwItemSelectionModel::cwItemSelectionModel(QObject *parent) :
    QItemSelectionModel(NULL, parent)
{
}

cwItemSelectionModel::cwItemSelectionModel(QAbstractItemModel *model, QObject *parent) :
    QItemSelectionModel(model, parent)
{
}
