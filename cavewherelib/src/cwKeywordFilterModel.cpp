#include "cwKeywordFilterModel.h"
#include "cwKeywordItemModel.h"

//Qt includes
#include <QDebug>

//Std includes
#include <algorithm>

cwKeywordFilterModel::cwKeywordFilterModel(QObject *parent) : QAbstractProxyModel(parent)
{
    qDebug() << "Created:" << this << parent;
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

    auto iter = std::find_if(mRows.cbegin(), mRows.cend(), [&sourceIndex](const Row& row) {
        return row.sourceIndex == sourceIndex;
    });

    if(iter == mRows.cend()) {
        return QModelIndex();
    }

    int row = static_cast<int>(std::distance(mRows.cbegin(), iter));
    return createIndex(row, 0);
}

void cwKeywordFilterModel::insert(const QModelIndex& sourceIndex)
{
    if(!sourceIndex.isValid()) {
        return;
    }

    QObject* object = toObject(sourceIndex);
    if(!object) {
        return;
    }

    auto iter = lowerBound(object);
    if(iter != mRows.end() && iter->object == object) {
        return;
    }

    int row = static_cast<int>(std::distance(mRows.begin(), iter));
    beginInsertRows(QModelIndex(), row, row);
    mRows.insert(iter, Row{object, QPersistentModelIndex(sourceIndex)});
    endInsertRows();

    // Remove automatically if the object goes away
    connect(object, &QObject::destroyed, this, [this](QObject* destroyed) {
        removeByObject(destroyed);
    });
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
        mRows.clear();
        endRemoveRows();
    }
}

void cwKeywordFilterModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    if(this->sourceModel() == sourceModel) {
        return;
    }

    disconnectSource();
    if(!mRows.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, mRows.size() - 1);
        mRows.clear();
        endRemoveRows();
    }

    QAbstractProxyModel::setSourceModel(sourceModel);

    if(sourceModel) {
        mRowsAboutToBeRemoved = connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved,
                                        this, [this, sourceModel](const QModelIndex& parent, int first, int last)
        {
            if(parent.isValid()) {
                return;
            }
            for(int i = first; i <= last; ++i) {
                remove(sourceModel->index(i, 0, parent));
            }
        });

        mDataChanged = connect(sourceModel, &QAbstractItemModel::dataChanged,
                               this, [this, sourceModel](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
        {
            if(!roles.contains(cwKeywordItemModel::ObjectRole)) {
                return;
            }

            if(topLeft.parent().isValid()) {
                return;
            }

            for(int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                auto srcIndex = sourceModel->index(row, 0, topLeft.parent());
                removeBySourceIndex(srcIndex);
                insert(srcIndex);
            }
        });
    }
}

QVector<cwKeywordFilterModel::Row>::iterator cwKeywordFilterModel::lowerBound(QObject *object)
{
    return std::lower_bound(mRows.begin(), mRows.end(), object,
                            [](const Row& row, QObject* o) { return row.object < o; });
}

QVector<cwKeywordFilterModel::Row>::const_iterator cwKeywordFilterModel::lowerBound(QObject *object) const
{
    return std::lower_bound(mRows.cbegin(), mRows.cend(), object,
                            [](const Row& row, QObject* o) { return row.object < o; });
}

void cwKeywordFilterModel::removeByObject(QObject *object)
{
    auto iter = lowerBound(object);
    if(iter == mRows.end() || iter->object != object) {
        return;
    }

    int row = static_cast<int>(std::distance(mRows.begin(), iter));
    beginRemoveRows(QModelIndex(), row, row);
    mRows.erase(iter);
    endRemoveRows();
}

void cwKeywordFilterModel::removeBySourceIndex(const QModelIndex &sourceIndex)
{
    auto iter = std::find_if(mRows.begin(), mRows.end(), [&sourceIndex](const Row& row) {
        return row.sourceIndex == sourceIndex;
    });

    if(iter == mRows.end()) {
        return;
    }

    int row = static_cast<int>(std::distance(mRows.begin(), iter));
    beginRemoveRows(QModelIndex(), row, row);
    mRows.erase(iter);
    endRemoveRows();
}

void cwKeywordFilterModel::disconnectSource()
{
    if(mDataChanged) {
        disconnect(mDataChanged);
        mDataChanged = {};
    }
    if(mRowsAboutToBeRemoved) {
        disconnect(mRowsAboutToBeRemoved);
        mRowsAboutToBeRemoved = {};
    }
}
