/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTableStaticColumnModel.h"

cwTableStaticColumnModel::cwTableStaticColumnModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

int cwTableStaticColumnModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_columns.size();
}

QVariant cwTableStaticColumnModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_columns.size()) {
        return QVariant();
    }

    const cwTableStaticColumn* column = m_columns.at(index.row());
    if (column == nullptr) {
        return QVariant();
    }

    switch (role) {
    case TextRole:
        return column->text();
    case ColumnWidthRole:
        return column->columnWidth();
    case SortRole:
        return column->sortRole();
    default:
        return QVariant();
    }
}

bool cwTableStaticColumnModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_columns.size()) {
        return false;
    }

    cwTableStaticColumn* column = m_columns.at(index.row());
    if (column == nullptr) {
        return false;
    }

    switch (role) {
    case ColumnWidthRole:
        column->setColumnWidth(value.toInt());
        return true;
    case TextRole:
        column->setText(value.toString());
        return true;
    case SortRole:
        column->setSortRole(value.toInt());
        return true;
    default:
        return false;
    }
}

Qt::ItemFlags cwTableStaticColumnModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> cwTableStaticColumnModel::roleNames() const
{
    return {
        {TextRole, "text"},
        {ColumnWidthRole, "columnWidth"},
        {SortRole, "sortRole"}
    };
}

QQmlListProperty<cwTableStaticColumn> cwTableStaticColumnModel::columns()
{
    return QQmlListProperty<cwTableStaticColumn>(this,
                                                 this,
                                                 &cwTableStaticColumnModel::appendColumn,
                                                 &cwTableStaticColumnModel::columnCount,
                                                 &cwTableStaticColumnModel::columnAt,
                                                 &cwTableStaticColumnModel::clearColumns);
}

int cwTableStaticColumnModel::count() const
{
    return m_columns.size();
}

int cwTableStaticColumnModel::totalWidth() const
{
    return m_totalWidth;
}

void cwTableStaticColumnModel::appendColumn(QQmlListProperty<cwTableStaticColumn>* list, cwTableStaticColumn* column)
{
    auto model = qobject_cast<cwTableStaticColumnModel*>(list->object);
    if (model == nullptr) {
        return;
    }

    model->insertColumn(model->m_columns.size(), column);
}

qsizetype cwTableStaticColumnModel::columnCount(QQmlListProperty<cwTableStaticColumn>* list)
{
    auto model = qobject_cast<cwTableStaticColumnModel*>(list->object);
    return model ? model->m_columns.size() : 0;
}

cwTableStaticColumn* cwTableStaticColumnModel::columnAt(QQmlListProperty<cwTableStaticColumn>* list, qsizetype index)
{
    auto model = qobject_cast<cwTableStaticColumnModel*>(list->object);
    if (model == nullptr || index < 0 || index >= model->m_columns.size()) {
        return nullptr;
    }

    return model->m_columns.at(index);
}

void cwTableStaticColumnModel::clearColumns(QQmlListProperty<cwTableStaticColumn>* list)
{
    auto model = qobject_cast<cwTableStaticColumnModel*>(list->object);
    if (model == nullptr) {
        return;
    }

    model->clearAllColumns();
}

void cwTableStaticColumnModel::insertColumn(int index, cwTableStaticColumn* column)
{
    if (column == nullptr) {
        return;
    }

    if (index < 0 || index > m_columns.size()) {
        index = m_columns.size();
    }

    beginInsertRows(QModelIndex(), index, index);
    m_columns.insert(index, column);
    if (column->parent() == nullptr) {
        column->setParent(this);
    }
    connectColumn(column);
    endInsertRows();

    updateTotalWidth();
    emit countChanged();
}

void cwTableStaticColumnModel::clearAllColumns()
{
    if (m_columns.isEmpty()) {
        return;
    }

    beginResetModel();
    m_columns.clear();
    endResetModel();

    updateTotalWidth();
    emit countChanged();
}

void cwTableStaticColumnModel::connectColumn(cwTableStaticColumn* column)
{
    connect(column, &cwTableStaticColumn::textChanged, this, [this, column]() {
        int row = columnIndex(column);
        if (row >= 0) {
            emit dataChanged(index(row, 0), index(row, 0), {TextRole});
        }
    });

    connect(column, &cwTableStaticColumn::columnWidthChanged, this, [this, column]() {
        updateTotalWidth();
        int row = columnIndex(column);
        if (row >= 0) {
            emit dataChanged(index(row, 0), index(row, 0), {ColumnWidthRole});
        }
    });

    connect(column, &cwTableStaticColumn::sortRoleChanged, this, [this, column]() {
        int row = columnIndex(column);
        if (row >= 0) {
            emit dataChanged(index(row, 0), index(row, 0), {SortRole});
        }
    });

}

void cwTableStaticColumnModel::updateTotalWidth()
{
    int totalWidth = 0;
    for (const cwTableStaticColumn* column : m_columns) {
        if (column != nullptr) {
            totalWidth += column->columnWidth();
        }
    }

    if (m_totalWidth == totalWidth) {
        return;
    }

    m_totalWidth = totalWidth;
    emit totalWidthChanged();
}

int cwTableStaticColumnModel::columnIndex(const cwTableStaticColumn* column) const
{
    return m_columns.indexOf(const_cast<cwTableStaticColumn*>(column));
}
