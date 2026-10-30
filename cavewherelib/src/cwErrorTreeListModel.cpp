/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwErrorTreeListModel.h"
#include "cwErrorListModel.h"

//Qt includes
#include <QSet>

//Std includes
#include <algorithm>
#include <utility>

cwErrorTreeListModel::cwErrorTreeListModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

void cwErrorTreeListModel::setSources(const QList<cwErrorListModel*>& sources)
{
    beginResetModel();
    for(const Source& source : m_sources) {
        disconnect(source.model, nullptr, this, nullptr);
    }
    m_sources.clear();
    m_positions.clear();
    m_rowCount = 0;

    m_sources.reserve(static_cast<size_t>(sources.size()));
    for(cwErrorListModel* model : sources) {
        if(!m_positions.contains(model)) {
            pushSource(model);
        }
    }
    endResetModel();
}

void cwErrorTreeListModel::appendSources(const QList<cwErrorListModel*>& sources)
{
    QList<cwErrorListModel*> added;
    QSet<const cwErrorListModel*> seen;
    int addedRows = 0;
    for(cwErrorListModel* model : sources) {
        if(m_positions.contains(model) || seen.contains(model)) {
            continue;
        }
        seen.insert(model);
        added.append(model);
        addedRows += model->size();
    }

    const bool hasRows = addedRows > 0;
    if(hasRows) {
        beginInsertRows(QModelIndex(), m_rowCount, m_rowCount + addedRows - 1);
    }
    for(cwErrorListModel* model : std::as_const(added)) {
        pushSource(model);
    }
    if(hasRows) {
        endInsertRows();
    }
}

void cwErrorTreeListModel::removeSources(const QList<cwErrorListModel*>& sources)
{
    std::vector<size_t> positions;
    positions.reserve(static_cast<size_t>(sources.size()));
    for(const cwErrorListModel* model : sources) {
        const auto found = m_positions.constFind(model);
        if(found != m_positions.cend()) {
            positions.push_back(found.value());
        }
    }
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());

    //Runs from the back, so the positions of earlier runs stay valid
    size_t runEnd = positions.size();
    while(runEnd > 0) {
        size_t runBegin = runEnd - 1;
        while(runBegin > 0 && positions.at(runBegin - 1) + 1 == positions.at(runBegin)) {
            --runBegin;
        }

        const size_t firstPosition = positions.at(runBegin);
        const size_t lastPosition = positions.at(runEnd - 1);
        const Source& last = m_sources.at(lastPosition);
        const int firstRow = m_sources.at(firstPosition).firstRow;
        const int rows = last.firstRow + last.rowCount - firstRow;

        const bool hasRows = rows > 0;
        if(hasRows) {
            beginRemoveRows(QModelIndex(), firstRow, firstRow + rows - 1);
        }

        const auto eraseBegin = m_sources.begin() + static_cast<std::ptrdiff_t>(firstPosition);
        const auto eraseEnd = m_sources.begin() + static_cast<std::ptrdiff_t>(lastPosition + 1);
        for(auto it = eraseBegin; it != eraseEnd; ++it) {
            disconnect(it->model, nullptr, this, nullptr);
            m_positions.remove(it->model);
        }
        m_sources.erase(eraseBegin, eraseEnd);
        m_rowCount -= rows;
        shiftFrom(firstPosition, -rows);
        reindexFrom(firstPosition);

        if(hasRows) {
            endRemoveRows();
        }
        runEnd = runBegin;
    }
}

QList<cwErrorListModel*> cwErrorTreeListModel::sources() const
{
    QList<cwErrorListModel*> models;
    models.reserve(static_cast<qsizetype>(m_sources.size()));
    for(const Source& source : m_sources) {
        models.append(source.model);
    }
    return models;
}

cwErrorListModel* cwErrorTreeListModel::sourceForRow(int row) const
{
    const Source* source = sourceAt(row);
    return source == nullptr ? nullptr : source->model;
}

cwError cwErrorTreeListModel::errorAt(int row) const
{
    const Source* source = sourceAt(row);
    if(source == nullptr) {
        return {};
    }
    return source->model->at(row - source->firstRow);
}

int cwErrorTreeListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rowCount;
}

QVariant cwErrorTreeListModel::data(const QModelIndex& index, int role) const
{
    const QModelIndex source = sourceIndex(index);
    return source.isValid() ? source.data(role) : QVariant();
}

bool cwErrorTreeListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(index.model() != this) {
        return false;
    }
    //The source's dataChanged comes back through onSourceDataChanged
    const Source* source = sourceAt(index.row());
    return source != nullptr
           && source->model->setData(source->model->index(index.row() - source->firstRow, 0), value, role);
}

Qt::ItemFlags cwErrorTreeListModel::flags(const QModelIndex& index) const
{
    const QModelIndex source = sourceIndex(index);
    return source.isValid() ? source.flags() : QAbstractListModel::flags(index);
}

QHash<int, QByteArray> cwErrorTreeListModel::roleNames() const
{
    //A QML view reads roles once, maybe before the first source arrives
    static const QHash<int, QByteArray> kListRoles = cwErrorListModel().roleNames();
    return kListRoles;
}

const cwErrorTreeListModel::Source* cwErrorTreeListModel::sourceAt(int row) const
{
    if(row < 0 || row >= m_rowCount) {
        return nullptr;
    }
    //The last source that starts at or before row; an empty source shares its
    //first row with the next one, which comes after it
    const auto after = std::upper_bound(m_sources.cbegin(), m_sources.cend(), row,
                                        [](int value, const Source& source) {
        return value < source.firstRow;
    });
    return &*std::prev(after);
}

QModelIndex cwErrorTreeListModel::sourceIndex(const QModelIndex& index) const
{
    if(index.model() != this) {
        return {};
    }
    const Source* source = sourceAt(index.row());
    return source == nullptr ? QModelIndex() : source->model->index(index.row() - source->firstRow, 0);
}

size_t cwErrorTreeListModel::senderPosition() const
{
    const auto found = m_positions.constFind(sender());
    Q_ASSERT(found != m_positions.cend());
    return found.value();
}

void cwErrorTreeListModel::pushSource(cwErrorListModel* model)
{
    const int rows = model->size();
    m_positions.insert(model, m_sources.size());
    m_sources.push_back({model, m_rowCount, rows});
    m_rowCount += rows;
    connectSource(model);
}

void cwErrorTreeListModel::resizeSenderSource(int rows)
{
    const size_t position = senderPosition();
    m_sources.at(position).rowCount += rows;
    shiftFrom(position + 1, rows);
    m_rowCount += rows;
}

void cwErrorTreeListModel::connectSource(cwErrorListModel* source)
{
    connect(source, &QAbstractItemModel::rowsAboutToBeInserted, this, &cwErrorTreeListModel::onSourceRowsAboutToBeInserted);
    connect(source, &QAbstractItemModel::rowsInserted, this, &cwErrorTreeListModel::onSourceRowsInserted);
    connect(source, &QAbstractItemModel::rowsAboutToBeRemoved, this, &cwErrorTreeListModel::onSourceRowsAboutToBeRemoved);
    connect(source, &QAbstractItemModel::rowsRemoved, this, &cwErrorTreeListModel::onSourceRowsRemoved);
    connect(source, &QAbstractItemModel::dataChanged, this, &cwErrorTreeListModel::onSourceDataChanged);
    connect(source, &QAbstractItemModel::modelAboutToBeReset, this, &cwErrorTreeListModel::onSourceAboutToBeReset);
    connect(source, &QAbstractItemModel::modelReset, this, &cwErrorTreeListModel::onSourceReset);
    //A list model's only layout change is a reorder, so a reset carries it
    connect(source, &QAbstractItemModel::layoutAboutToBeChanged, this, &cwErrorTreeListModel::onSourceAboutToBeReset);
    connect(source, &QAbstractItemModel::layoutChanged, this, &cwErrorTreeListModel::onSourceReset);
}

void cwErrorTreeListModel::shiftFrom(size_t position, int rows)
{
    for(size_t i = position; i < m_sources.size(); ++i) {
        m_sources.at(i).firstRow += rows;
    }
}

void cwErrorTreeListModel::reindexFrom(size_t position)
{
    for(size_t i = position; i < m_sources.size(); ++i) {
        m_positions.insert(m_sources.at(i).model, i);
    }
}

void cwErrorTreeListModel::onSourceRowsAboutToBeInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent);
    const Source& source = m_sources.at(senderPosition());
    beginInsertRows(QModelIndex(), source.firstRow + first, source.firstRow + last);
}

void cwErrorTreeListModel::onSourceRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent);
    resizeSenderSource(last - first + 1);
    endInsertRows();
}

void cwErrorTreeListModel::onSourceRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent);
    const Source& source = m_sources.at(senderPosition());
    beginRemoveRows(QModelIndex(), source.firstRow + first, source.firstRow + last);
}

void cwErrorTreeListModel::onSourceRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent);
    resizeSenderSource(-(last - first + 1));
    endRemoveRows();
}

void cwErrorTreeListModel::onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
{
    const Source& source = m_sources.at(senderPosition());
    emit dataChanged(index(source.firstRow + topLeft.row()),
                     index(source.firstRow + bottomRight.row()),
                     roles);
}

void cwErrorTreeListModel::onSourceAboutToBeReset()
{
    beginResetModel();
}

void cwErrorTreeListModel::onSourceReset()
{
    const Source& source = m_sources.at(senderPosition());
    resizeSenderSource(source.model->size() - source.rowCount);
    endResetModel();
}
