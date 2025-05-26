#include "TextModel.h"

using namespace cwSketch;

int TextModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_rows.size();
}

QVariant TextModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
    {
        return {};
    }

    const TextRow& row = m_rows.at(index.row());
    switch (role)
    {
    case TextRole:        return row.text;
    case PositionRole:    return row.position;
    case FontRole:        return row.font;
    case FillColorRole:   return row.fillColor;
    case StrokeColorRole: return row.strokeColor;
    default:              return {};
    }
}

bool TextModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
    {
        return false;
    }

    TextRow& row = m_rows[index.row()];
    switch (role)
    {
    case TextRole:
        row.text = value.toString();
        break;
    case PositionRole:
        row.position = value.toPointF();
        break;
    case FontRole:
        row.font = qvariant_cast<QFont>(value);
        break;
    case FillColorRole:
        row.fillColor = qvariant_cast<QColor>(value);
        break;
    case StrokeColorRole:
        row.strokeColor = qvariant_cast<QColor>(value);
        break;
    default:
        return false;
    }

    emit dataChanged(index, index, { role });
    return true;
}

QHash<int, QByteArray> TextModel::roleNames() const
{
    return {
        { TextRole, "textRole" },
        { PositionRole, "positionRole" },
        { FontRole, "fontRole" },
        { FillColorRole, "fillColorRole" },
        { StrokeColorRole, "strokeColorRole" }
    };
}

void TextModel::addText(const QModelIndex &index, const TextRow &row)
{
    int pos = index.isValid() ? index.row() : m_rows.size();
    beginInsertRows(QModelIndex(), pos, pos);
    m_rows.insert(pos, row);
    endInsertRows();
}

void TextModel::removeText(const QModelIndex &index)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
    {
        return;
    }

    beginRemoveRows(QModelIndex(), index.row(), index.row());
    m_rows.remove(index.row());
    endRemoveRows();
}

void TextModel::setRows(const QVector<TextRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

void TextModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}
