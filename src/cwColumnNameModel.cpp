#include "cwColumnNameModel.h"

cwColumnNameModel::cwColumnNameModel(QObject* parent) :
    QAbstractListModel(parent)
{

}

/**
 * Returns the number of "Columns" in the model
 */
int cwColumnNameModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Columns.size();
}

/**
 * Returns the data for each column in the model.
 */
QVariant cwColumnNameModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    auto column = Columns.at(index.row());

    switch(role) {
    case NameRole:
        return column.Name;
    case IdRole:
        return column.Id;
    }

    return QVariant();
}

/**
 * Returns the role names for the model
 */
QHash<int, QByteArray> cwColumnNameModel::roleNames() const
{
    return {
        {NameRole, "nameRole"},
        {IdRole, "idRole"}
    };
}

/**
 * This set the columns that will make up the rows of the model
 *
 * This will reset the model with columns
 */
void cwColumnNameModel::setColumns(const QVector<cwColumnNameModel::Column> columns)
{
    beginResetModel();
    Columns = columns;
    endResetModel();
}
