/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalSourceModel.h"

//Std includes
#include <algorithm>

cwExternalSourceModel::cwExternalSourceModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

int cwExternalSourceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_sources.size();
}

QVariant cwExternalSourceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sources.size()) {
        return QVariant();
    }

    const cwExternalSource& source = m_sources.at(index.row());
    switch (role) {
    case SourceRole:              return QVariant::fromValue(source);
    case IdRole:                  return source.id();
    case RootNodeIdRole:          return source.rootNodeId();
    case ExcludedSourcePathsRole: return source.excludedSourcePaths();
    default:                      return QVariant();
    }
}

QHash<int, QByteArray> cwExternalSourceModel::roleNames() const
{
    return {
        {SourceRole,              "source"},
        {IdRole,                  "sourceId"},
        {RootNodeIdRole,          "rootNodeId"},
        {ExcludedSourcePathsRole, "excludedSourcePaths"}
    };
}

void cwExternalSourceModel::appendSource(const cwExternalSource& source)
{
    const int row = m_sources.size();
    beginInsertRows(QModelIndex(), row, row);
    m_sources.append(source);
    endInsertRows();
    emit countChanged();
}

void cwExternalSourceModel::removeAt(int index)
{
    if (index < 0 || index >= m_sources.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), index, index);
    m_sources.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

void cwExternalSourceModel::replaceAt(int index, const cwExternalSource& source)
{
    if (index < 0 || index >= m_sources.size() || m_sources.at(index) == source) {
        return;
    }
    m_sources.replace(index, source);
    const QModelIndex changed = this->index(index);
    emit dataChanged(changed, changed);
}

cwExternalSource cwExternalSourceModel::sourceAt(int index) const
{
    if (index < 0 || index >= m_sources.size()) {
        return cwExternalSource();
    }
    return m_sources.at(index);
}

int cwExternalSourceModel::indexOf(const QUuid& id) const
{
    const auto found = std::find_if(m_sources.cbegin(), m_sources.cend(),
                                    [&id](const cwExternalSource& source) {
        return source.id() == id;
    });
    return found == m_sources.cend() ? -1 : static_cast<int>(found - m_sources.cbegin());
}

void cwExternalSourceModel::setSources(const QList<cwExternalSource>& sources)
{
    const bool sizeChanged = (m_sources.size() != sources.size());
    beginResetModel();
    m_sources = sources;
    endResetModel();
    if (sizeChanged) {
        emit countChanged();
    }
}
