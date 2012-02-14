#include "cwItemSelectionModel.h"

cwItemSelectionModel::cwItemSelectionModel(QObject *parent) :
    QItemSelectionModel(NULL, parent)
{
}

cwItemSelectionModel::cwItemSelectionModel(QAbstractItemModel *model, QObject *parent) :
    QItemSelectionModel(model, parent)
{
}
