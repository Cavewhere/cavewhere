#include "cwKeywordFilterModel.h"
#include "cwKeywordItemModel.h"

//Qt includes
#include <QDebug>
#include <QSet>

//Std includes
#include <algorithm>

cwKeywordFilterModel::cwKeywordFilterModel(QObject *parent) : QAbstractProxyModel(parent)
{
    // qDebug() << "Created:" << this << parent;
}

QModelIndex cwKeywordFilterModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if(row >= 0 && row < mRows.size()) {
        return createIndex(row, 0);
    }
    return QModelIndex();
}

QModelIndex cwKeywordFilterModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int cwKeywordFilterModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return mRows.size();
}

int cwKeywordFilterModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QModelIndex cwKeywordFilterModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if(proxyIndex.isValid()) {
        return mRows.at(proxyIndex.row()).sourceIndex;
    }
    return QModelIndex();
}

QModelIndex cwKeywordFilterModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if(!sourceIndex.isValid()) {
        return QModelIndex();
    }

    const int row = mRowOfObject.value(toObject(sourceIndex), -1);
    if(row < 0 || mRows.at(row).sourceIndex != sourceIndex) {
        return QModelIndex();
    }
    return createIndex(row, 0);
}

void cwKeywordFilterModel::insert(const QModelIndex& sourceIndex)
{
    insert(QList<QModelIndex>{sourceIndex});
}

/**
 * Appends a row for each source index whose object is not already a row, with
 * one insert over the new range.
 */
void cwKeywordFilterModel::insert(const QList<QModelIndex>& sourceIndexes)
{
    QVector<Row> added;
    QSet<QObject*> seen;
    for(const QModelIndex& sourceIndex : sourceIndexes) {
        if(!sourceIndex.isValid()) {
            continue;
        }

        QObject* object = toObject(sourceIndex);
        if(!object || mRowOfObject.contains(object) || seen.contains(object)) {
            continue;
        }
        seen.insert(object);
        added.append(Row{object, QPersistentModelIndex(sourceIndex), {}});
    }

    if(added.isEmpty()) {
        return;
    }

    const int first = static_cast<int>(mRows.size());
    beginInsertRows(QModelIndex(), first, first + static_cast<int>(added.size()) - 1);
    mRows.reserve(first + added.size());
    for(Row& row : added) {
        // Remove automatically if the object goes away
        row.destroyedConnection = connect(row.object, &QObject::destroyed, this, [this](QObject* destroyed) {
            removeByObject(destroyed);
        });
        mRowOfObject.insert(row.object, static_cast<int>(mRows.size()));
        mRows.append(row);
    }
    endInsertRows();
}

void cwKeywordFilterModel::remove(const QModelIndex& sourceIndex)
{
    if(!sourceIndex.isValid()) {
        return;
    }
    QObject* object = toObject(sourceIndex);
    if(!object) {
        return;
    }

    removeByObject(object);
}

void cwKeywordFilterModel::clear()
{
    if(!mRows.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, mRows.size() - 1);
        for(const Row& row : std::as_const(mRows)) {
            disconnect(row.destroyedConnection);
        }
        mRows.clear();
        mRowOfObject.clear();
        endRemoveRows();
    }
}

void cwKeywordFilterModel::eraseRow(int row)
{
    disconnect(mRows.at(row).destroyedConnection);
    beginRemoveRows(QModelIndex(), row, row);
    mRowOfObject.remove(mRows.at(row).object);
    mRows.removeAt(row);
    for(int i = row; i < mRows.size(); ++i) {
        mRowOfObject.insert(mRows.at(i).object, i);
    }
    endRemoveRows();
}

void cwKeywordFilterModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    if(this->sourceModel() == sourceModel) {
        return;
    }

    disconnectSource();
    clear();

    QAbstractProxyModel::setSourceModel(sourceModel);

    if(sourceModel) {
        mSourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved,
                                          this, [this, sourceModel](const QModelIndex& parent, int first, int last)
        {
            if(parent.isValid()) {
                return;
            }
            for(int i = first; i <= last; ++i) {
                remove(sourceModel->index(i, 0, parent));
            }
        }));

        mSourceConnections.append(connect(sourceModel, &QAbstractItemModel::dataChanged,
                                          this, [this, sourceModel](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
        {
            if(topLeft.parent().isValid()) {
                // Child rows are an item's keywords.
                if(roles.contains(cwKeywordItemModel::KeywordsRole)) {
                    forwardKeywordsChanged(topLeft.parent());
                }
                return;
            }

            if(roles.contains(cwKeywordItemModel::ObjectRole)) {
                for(int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                    auto srcIndex = sourceModel->index(row, 0);
                    removeBySourceIndex(srcIndex);
                    insert(srcIndex);
                }
            } else if(roles.contains(cwKeywordItemModel::KeywordsRole)) {
                for(int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                    forwardKeywordsChanged(sourceModel->index(row, 0));
                }
            }
        }));

        const auto forwardChildRows = [this](const QModelIndex& parent) {
            if(parent.isValid()) {
                forwardKeywordsChanged(parent);
            }
        };
        mSourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsInserted,
                                          this, forwardChildRows));
        mSourceConnections.append(connect(sourceModel, &QAbstractItemModel::rowsRemoved,
                                          this, forwardChildRows));

        // Clear while the source's rows are still readable.
        mSourceConnections.append(connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset,
                                          this, &cwKeywordFilterModel::clear));
    }
}

void cwKeywordFilterModel::forwardKeywordsChanged(const QModelIndex& sourceIndex)
{
    const QModelIndex proxyIndex = mapFromSource(sourceIndex);
    if(proxyIndex.isValid()) {
        emit dataChanged(proxyIndex, proxyIndex, {cwKeywordItemModel::KeywordsRole});
    }
}

void cwKeywordFilterModel::removeByObject(QObject *object)
{
    const int row = mRowOfObject.value(object, -1);
    if(row < 0) {
        return;
    }

    eraseRow(row);
}

void cwKeywordFilterModel::removeBySourceIndex(const QModelIndex &sourceIndex)
{
    // Linear: called when the source index's object changed, so the object
    // lookup no longer finds its row.
    auto iter = std::find_if(mRows.cbegin(), mRows.cend(), [&sourceIndex](const Row& row) {
        return row.sourceIndex == sourceIndex;
    });

    if(iter == mRows.cend()) {
        return;
    }

    eraseRow(static_cast<int>(std::distance(mRows.cbegin(), iter)));
}

void cwKeywordFilterModel::disconnectSource()
{
    for(const QMetaObject::Connection& connection : std::as_const(mSourceConnections)) {
        disconnect(connection);
    }
    mSourceConnections.clear();
}
