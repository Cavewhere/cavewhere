/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGridTextModel.h"

//Qt includes
#include <QDebug>

int cwGridTextModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwGridTextModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const TextRow &row = m_rows.at(index.row());
    switch (role) {
    case TextRole:        return row.text;
    case PositionRole:    return row.position;
    case FontRole:        return row.font;
    case FillColorRole:   return row.fillColor;
    case StrokeColorRole: return row.strokeColor;
    default:              return {};
    }
}

bool cwGridTextModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return false;
    }

    TextRow &row = m_rows[index.row()];
    switch (role) {
    case TextRole:        row.text = value.toString(); break;
    case PositionRole:    row.position = value.toPointF(); break;
    case FontRole:        row.font = qvariant_cast<QFont>(value); break;
    case FillColorRole:   row.fillColor = qvariant_cast<QColor>(value); break;
    case StrokeColorRole: row.strokeColor = qvariant_cast<QColor>(value); break;
    default:              return false;
    }

    emit dataChanged(index, index, { role });
    return true;
}

QHash<int, QByteArray> cwGridTextModel::roleNames() const
{
    return staticRoleNames();
}

QHash<int, QByteArray> cwGridTextModel::staticRoleNames()
{
    return {
        { TextRole,        "textRole" },
        { PositionRole,    "positionRole" },
        { FontRole,        "fontRole" },
        { FillColorRole,   "fillColorRole" },
        { StrokeColorRole, "strokeColorRole" }
    };
}

void cwGridTextModel::addText(int rowIndex, const TextRow &row)
{
    if (rowIndex < 0 || rowIndex > m_rows.size()) {
        Q_ASSERT(false);
        return;
    }

    beginInsertRows(QModelIndex(), rowIndex, rowIndex);
    m_rows.insert(rowIndex, row);
    endInsertRows();
}

void cwGridTextModel::addText(int rowIndex, const QVector<TextRow> &rows)
{
    if (rowIndex < 0 || rowIndex > m_rows.size() || rows.isEmpty()) {
        Q_ASSERT(false);
        return;
    }

    const int pos = rowIndex;
    const int count = rows.size();

    beginInsertRows(QModelIndex(), pos, pos + count - 1);
    for (int i = 0; i < count; ++i) {
        m_rows.insert(pos + i, rows.at(i));
    }
    endInsertRows();
}

void cwGridTextModel::removeText(const QModelIndex &index)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        Q_ASSERT(false);
        return;
    }

    beginRemoveRows(QModelIndex(), index.row(), index.row());
    m_rows.remove(index.row());
    endRemoveRows();
}

void cwGridTextModel::removeText(const QModelIndex &index, int count)
{
    if (!index.isValid()
        || index.row() < 0
        || index.row() >= m_rows.size()
        || count <= 0)
    {
        Q_ASSERT(false);
        return;
    }

    const int start = index.row();
    const int end = start + count - 1;

    Q_ASSERT(end <= m_rows.size() - 1);

    beginRemoveRows(QModelIndex(), start, end);
    for (int i = start; i <= end; ++i) {
        m_rows.remove(start);
    }
    endRemoveRows();
}

void cwGridTextModel::setRows(const QVector<TextRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

/**
 * Prefers insert/remove + dataChanged over a full resetModel so QML views
 * can re-use delegate items instead of destroying/recreating them.
 */
void cwGridTextModel::replaceRows(const QVector<TextRow> &rows)
{
    if (m_rows.size() < rows.size()) {
        const int diff = rows.size() - m_rows.size();
        const int begin = m_rows.size();
        beginInsertRows(QModelIndex(), begin, begin + diff - 1);
        m_rows = rows;
        endInsertRows();

        Q_ASSERT(!m_rows.isEmpty());
        if (begin > 0) {
            emit dataChanged(index(0), index(begin - 1));
        }
    } else if (m_rows.size() > rows.size()) {
        const int diff = m_rows.size() - rows.size();
        const int begin = m_rows.size() - diff;
        beginRemoveRows(QModelIndex(), begin, m_rows.size() - 1);
        m_rows = rows;
        endRemoveRows();

        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0), index(m_rows.size() - 1));
        }
    } else {
        m_rows = rows;

        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0), index(m_rows.size() - 1));
        }
    }
}

void cwGridTextModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}
