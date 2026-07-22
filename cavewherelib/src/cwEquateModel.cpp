/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwEquateModel.h"

cwEquateModel::cwEquateModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

cwEquateModel::~cwEquateModel() = default;

int cwEquateModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_equates.size();
}

QModelIndex cwEquateModel::index(int row, int column, const QModelIndex& parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

QVariant cwEquateModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_equates.size()) {
        return QVariant();
    }

    const cwEquate& equate = m_equates.at(index.row());
    switch (role) {
    case EquateRole:        return QVariant::fromValue(equate);
    case StationCountRole:  return equate.stations().size();
    default:                return QVariant();
    }
}

QHash<int, QByteArray> cwEquateModel::roleNames() const
{
    return {
        {EquateRole,       "equate"},
        {StationCountRole, "stationCount"}
    };
}

void cwEquateModel::appendEquate(const cwEquate& equate)
{
    const int row = m_equates.size();
    beginInsertRows(QModelIndex(), row, row);
    m_equates.append(equate);
    endInsertRows();
    emit countChanged();
}

void cwEquateModel::removeAt(int index)
{
    if (index < 0 || index >= m_equates.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), index, index);
    m_equates.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

cwEquate cwEquateModel::equateAt(int index) const
{
    if (index < 0 || index >= m_equates.size()) {
        return cwEquate();
    }
    return m_equates.at(index);
}

void cwEquateModel::setEquates(const QList<cwEquate>& equates)
{
    const bool sizeChanged = (m_equates.size() != equates.size());
    beginResetModel();
    m_equates = equates;
    endResetModel();
    if (sizeChanged) {
        emit countChanged();
    }
}
