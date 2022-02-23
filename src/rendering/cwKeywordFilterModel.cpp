#include "cwKeywordFilterModel.h"
#include "cwKeywordItemModel.h"

//Qt includes
#include <QDebug>

//Std includes
#include <algorithm>

cwKeywordFilterModel::cwKeywordFilterModel(QObject *parent) : QAbstractProxyModel(parent)
{
    connect(this, &cwKeywordFilterModel::sourceModelChanged,
            this, [this]()
    {
        disconnect(mDataChanged);
        disconnect(mRowsAboutToBeRemoved);

        if(sourceModel()) {
            mDataChanged = connect(sourceModel(), &QAbstractItemModel::dataChanged,
                                   this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
            {
                //Update the index when the object has changed
                if(roles.contains(cwKeywordItemModel::ObjectRole)) {
                    for(int row = topLeft.row(); row <= bottomRight.row(); row++) {
                        auto sourceIndex = sourceModel()->index(row, 0, topLeft.parent());
                        int oldRow = mAcceptedSourceIndexes.indexOf(sourceIndex);
                        beginRemoveRows(QModelIndex(), oldRow, oldRow);
                        mAcceptedSourceIndexes.removeAt(oldRow);
                        endRemoveRows();

                        //Re-accept the sourceIndex
                        insert(sourceIndex);
                    }
                }
            });

            mRowsAboutToBeRemoved = connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved,
                                            this, [this](const QModelIndex& parent, int begin, int last)
            {
                if(parent == QModelIndex()) {
                    for(int i = begin; i <= last; i++) {
                        auto sourceIndex = sourceModel()->index(i, 0, QModelIndex());
                        remove(sourceIndex);
                    }
                }
            });
        }
    });
}


QModelIndex cwKeywordFilterModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if(row >= 0 && row < mAcceptedSourceIndexes.size()) {
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
    return mAcceptedSourceIndexes.size();
}

int cwKeywordFilterModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QModelIndex cwKeywordFilterModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if(proxyIndex.isValid()) {
        return mAcceptedSourceIndexes.at(proxyIndex.row());
    }
    return QModelIndex();
}

QModelIndex cwKeywordFilterModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    return findElementRunAction<QModelIndex>(sourceIndex,
                                             std::equal_to<QObject*>(),
                                             [this, &sourceIndex](int row, auto iter)
    {
        Q_UNUSED(iter);
        return createIndex(row, sourceIndex.column());
    });
}

void cwKeywordFilterModel::insert(const QModelIndex& sourceIndex)
{
    findElementRunAction<void>(sourceIndex,
                               std::not_equal_to<QObject*>(),
                               [this, &sourceIndex](int row, QVector<QPersistentModelIndex>::iterator iter)
    {
        beginInsertRows(QModelIndex(), row, row);
        mAcceptedSourceIndexes.insert(iter, sourceIndex);
        endInsertRows();
    });
}

void cwKeywordFilterModel::remove(const QModelIndex& sourceIndex)
{
    findElementRunAction<void>(sourceIndex,
                               std::equal_to<QObject*>(),
                               [this, sourceIndex](int row, auto iter)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mAcceptedSourceIndexes.erase(iter);
        endRemoveRows();
    });
}

QVector<QPersistentModelIndex>::iterator cwKeywordFilterModel::findAcceptedObject(QObject *object)
{
    return std::lower_bound(mAcceptedSourceIndexes.begin(),
                            mAcceptedSourceIndexes.end(),
                            object,
                            [](const QPersistentModelIndex& modelIndex, QObject* obj)
    {
        Q_ASSERT(modelIndex.isValid());
        return cwKeywordFilterModel::toObject(modelIndex) < obj;
    });
}

QVector<QPersistentModelIndex>::const_iterator cwKeywordFilterModel::findAcceptedObject(QObject *object) const
{
    //Use the non-const version
    return const_cast<QVector<QPersistentModelIndex>::const_iterator>(const_cast<cwKeywordFilterModel*>(this)->findAcceptedObject(object));
}






