#include "cwColumnNameModel.h"

cwColumnNameModel::cwColumnNameModel(QObject* parent) :
    QAbstractListModel(parent)
{
    connect(this, &cwColumnNameModel::rowsInserted, this, &cwColumnNameModel::countChanged);
    connect(this, &cwColumnNameModel::rowsRemoved, this, &cwColumnNameModel::countChanged);
}

int cwColumnNameModel::rowCount(const QModelIndex &parent) const
{
    return m_columnNames.size();
}

QVariant cwColumnNameModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= m_columnNames.size()) {
        return QVariant();
    }

    switch (role) {
        case static_cast<int>(ColumnRoles::Name):
            return m_columnNames.at(index.row()).name();
        case static_cast<int>(ColumnRoles::ColumnId):
            return m_columnNames.at(index.row()).columnId();
    }

    return QVariant();
}

QHash<int, QByteArray> cwColumnNameModel::roleNames() const
{
    return {
        {static_cast<int>(ColumnRoles::Name), "name"},
        {static_cast<int>(ColumnRoles::ColumnId), "columnId"}
    };
}

void cwColumnNameModel::append(const cwColumnName &columnName)
{
    beginInsertRows(QModelIndex(), m_columnNames.size(), m_columnNames.size());
    m_columnNames.append(columnName);
    endInsertRows();
}

void cwColumnNameModel::append(const QList<cwColumnName> &columnNames)
{
    if (columnNames.isEmpty()) {
        return;
    }

    beginInsertRows(QModelIndex(), m_columnNames.size(), m_columnNames.size() + columnNames.size() - 1);
    m_columnNames.append(columnNames);
    endInsertRows();
}

void cwColumnNameModel::insert(int index, const cwColumnName &columnName)
{
    beginInsertRows(QModelIndex(), index, index);
    m_columnNames.insert(index, columnName);
    endInsertRows();
}

void cwColumnNameModel::insert(int index, const QList<cwColumnName> &columnNames)
{
    if (columnNames.isEmpty()) {
        return;
    }

    beginInsertRows(QModelIndex(), index, index + columnNames.size() - 1);
    for (const cwColumnName &columnName : columnNames) {
        m_columnNames.insert(index, columnName);
    }
    endInsertRows();
}

void cwColumnNameModel::remove(int index)
{
    if (index < 0 || index >= m_columnNames.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_columnNames.removeAt(index);
    endRemoveRows();
}

void cwColumnNameModel::clear()
{
    beginResetModel();
    m_columnNames.clear();
    endResetModel();
}

cwColumnName cwColumnNameModel::get(int index) const
{
    if(index < 0 && index >= m_columnNames.size()) {
        return cwColumnName();
    }

    return m_columnNames.at(index);
}

int cwColumnNameModel::count() const
{
    return rowCount();
}
