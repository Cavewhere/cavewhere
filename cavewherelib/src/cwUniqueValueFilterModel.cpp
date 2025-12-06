#include "cwUniqueValueFilterModel.h"

#include <QAbstractItemModel>
#include <QDebug>
#include <algorithm>

cwUniqueValueFilterModel::cwUniqueValueFilterModel(QObject *parent) : QAbstractProxyModel(parent)
{
}

void cwUniqueValueFilterModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    if(this->sourceModel() != sourceModel) {
        disconnectSourceModel();

        QAbstractProxyModel::setSourceModel(sourceModel);

        buildIndex();
    }
}


QModelIndex cwUniqueValueFilterModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(column);
    Q_UNUSED(parent);
    return createIndex(row, 0);
}

QModelIndex cwUniqueValueFilterModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int cwUniqueValueFilterModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return mUniqueIndex.size();
}

int cwUniqueValueFilterModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 0;
}

QVariant cwUniqueValueFilterModel::data(const QModelIndex &index, int role) const
{
    if(index.isValid() && index.row() < mUniqueIndex.size()) {

        // if(mUniqueIndex.at(index.row()).index.data(role) == QVariant()) {
            // qDebug() << "Concat role names:" << mUniqueIndex.at(index.row()).index.model()->roleNames();

            // for(int i = 0; i < mUniqueIndex.at(index.row()).index.model()->rowCount(); i++) {
            //     qDebug() << "Concat:" << i << mUniqueIndex.at(index.row()).index.model()->index(i, 0).data(role) << "value:" << mUniqueIndex.at(index.row()).value;
            // }

        // }
        return mUniqueIndex.at(index.row()).index.data(role);
    }
    qDebug() << "Returning QVariant!" << index << role;
    return QVariant();
}

QModelIndex cwUniqueValueFilterModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if(proxyIndex.isValid()) {
        return mUniqueIndex.at(proxyIndex.row()).index;
    }
    return QModelIndex();
}

QModelIndex cwUniqueValueFilterModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    return findRunAction<QModelIndex>(sourceIndex,
                                      [this, sourceIndex](auto iter) { return contains(iter, sourceIndex); },
    [this](auto iter) {
        return index(iteratorToRow(iter));
    });
}

void cwUniqueValueFilterModel::buildIndex()
{
    if(!mUniqueIndex.isEmpty() || isValid()) {
        beginResetModel();
        mUniqueIndex.clear();
        if(isValid()) {
            if(!isConnected) {
                connectSourceModel();
            }

            for(int i = 0; i < sourceModel()->rowCount(); i++) {
                const auto index = sourceModel()->index(i, 0);

                findRunAction<void>(index,
                                    [this, index](auto iter) { return !contains(iter, index); },
                [this, index](auto iter) {
                    mUniqueIndex.insert(iter, toRow(index));
                });
            }
        } else {
            disconnectSourceModel();
        }
        endResetModel();
    }
}

void cwUniqueValueFilterModel::insertIntoIndex(const QModelIndex &sourceIndex)
{
    auto iter = findProxyIndex(sourceIndex);
    if(iter == mUniqueIndex.end() || iter->index != sourceIndex) {
        int row = iteratorToRow(iter);
        beginInsertRows(QModelIndex(), row, row);
        mUniqueIndex.insert(iter, toRow(sourceIndex));
        endInsertRows();
    }
}

QVector<cwUniqueValueFilterModel::Row>::const_iterator cwUniqueValueFilterModel::findProxyIndex(const QModelIndex &sourceIndex) const
{
    return findProxyIndex(uniqueValue(sourceIndex));
}

QVector<cwUniqueValueFilterModel::Row>::iterator cwUniqueValueFilterModel::findProxyIndex(const QModelIndex &sourceIndex)
{
    return findProxyIndex(uniqueValue(sourceIndex));
}


QVector<cwUniqueValueFilterModel::Row>::const_iterator cwUniqueValueFilterModel::findProxyIndex(const QVariant &value) const
{
    return std::lower_bound(mUniqueIndex.cbegin(),
                            mUniqueIndex.cend(),
                            value,
                            [this](const Row& row, const QVariant& value)
    {
        return mLessThan(row.value, value);
    });
}

QVector<cwUniqueValueFilterModel::Row>::iterator cwUniqueValueFilterModel::findProxyIndex(const QVariant &value)
{
    return std::lower_bound(mUniqueIndex.begin(),
                            mUniqueIndex.end(),
                            value,
                            [this](const Row& row, const QVariant& value)
    {
        return mLessThan(row.value, value);
    });
}

void cwUniqueValueFilterModel::connectSourceModel()
{
    if(!sourceModel()) {
        return;
    }

    auto insert = [this](const QModelIndex& sourceIndex) {
        Q_ASSERT(sourceIndex.isValid());

        // qDebug() << "cwUniqueValueFilterModel: insert" << sourceIndex;

        findRunAction<void>(sourceIndex,
                            [this, sourceIndex](auto iter) { return !contains(iter, sourceIndex); },
        [this, sourceIndex](auto iter)
        {
            iter = mUniqueIndex.insert(iter, toRow(sourceIndex));
            auto row = iteratorToRow(iter);

            beginInsertRows(QModelIndex(), row, row);
            endInsertRows();
        });
    };

    auto remove = [this](auto iter, const QModelIndex& sourceIndex) {
        auto findNextIndex = [this, sourceIndex](const QVariant& value)
        {
            auto n = this->sourceModel()->rowCount();
            for(int i = 0; i < n; i++) {
                auto currentIndex = this->sourceModel()->index(i, 0);
                if(currentIndex != sourceIndex) {
                    if(value == uniqueValue(currentIndex)) {
                        //Found replacement index
                        return currentIndex;
                    }
                }
            }

            return QModelIndex();
        };

        auto nextIndex = findNextIndex(iter->value);
        if(nextIndex == QModelIndex()) {
            //The only index in the whole set
            int row = iteratorToRow(iter);
            beginRemoveRows(QModelIndex(), row, row);
            mUniqueIndex.erase(iter);
            endRemoveRows();
        } else {
            //There's another index that has the same value, replace
            //Don't need to datachange since it's the same??
            iter->index = nextIndex;
        }
    };

    connect(sourceModel(), &QAbstractItemModel::rowsInserted,
            this, [this, insert](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent);

        //This is a work around to the row inconsistancy in sourceModel()
        if(sourceModel()->columnCount() <= 0) {
            return;
        }

        // Q_ASSERT(sourceModel() == sender()); //if this fails, we haven't disconnected a old sourceModel()
        // qDebug() << "SourcModel:" << sourceModel() << sourceModel()->rowCount() << sourceModel()->columnCount() << sourceModel()->hasIndex(0, 0);
        // qDebug() << "Insert:" << first << last;

        for(int i = first; i <= last; i++) {
            const auto index = this->sourceModel()->index(i, 0);
            insert(index);
        }
    });

    //This is to handle concatination model inconsistancy with column rows
    connect(sourceModel(), &QAbstractItemModel::columnsInserted,
            this, [this, insert](const QModelIndex& parent, int first, int last) {

        if(sourceModel()->rowCount() <= 0) {
            return;
        }

        if(mUniqueIndex.isEmpty()) {
            // qDebug() << "Running with column change";

            //Only run on new models
            for(int i = 0; i < sourceModel()->rowCount(); i++) {
                const auto index = this->sourceModel()->index(i, 0);
                insert(index);
            }
        }
    });

    connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved,
            this, [this, remove](const QModelIndex& parent, int first, int last)
    {
        Q_ASSERT(sourceModel() == sender()); //if this fails, we haven't disconnected a old sourceModel()


        Q_UNUSED(parent);
        for(int i = first; i <= last; i++) {
            const auto sourceIndexToRemove = this->sourceModel()->index(i, 0);
            this->findRunAction<void>(sourceIndexToRemove,
                                [this, sourceIndexToRemove](auto iter)
            {
                return contains(iter, sourceIndexToRemove);
            },

            [sourceIndexToRemove, remove](auto iter)
            {
                remove(iter, sourceIndexToRemove);
            });
        }
    });

    connect(sourceModel(), &QAbstractItemModel::modelAboutToBeReset,
            this, [this]()
    {
        qDebug() << "Model reset" << sourceModel()->rowCount();
    });

    connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeMoved,
            this, [](const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
            const QModelIndex &destinationParent, int destinationRow)
    {
        Q_UNUSED(sourceParent);
        Q_UNUSED(sourceStart);
        Q_UNUSED(sourceEnd);
        Q_UNUSED(destinationParent);
        Q_UNUSED(destinationRow);
        qDebug() << "Rows moved";
    });

    connect(sourceModel(), &QAbstractItemModel::dataChanged,
            this, [this, insert, remove](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int> roles)
    {
        Q_UNUSED(topLeft);
        Q_UNUSED(bottomRight);
        if(roles.contains(mUniqueRole)) {
            for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
                const auto sourceIndex = this->sourceModel()->index(i, 0);

                //Did we index the old value
                auto iter = std::find_if(mUniqueIndex.begin(), mUniqueIndex.end(),
                                                     [sourceIndex](const Row& row)
                {
                    return row.index == sourceIndex;
                });

                Q_ASSERT(iter != mUniqueIndex.end());
                //Is in the index, but probably in the wrong spot.
                //Remove it.
                remove(iter, sourceIndex);

                //Try to insert the sourceIndex back into the model
                insert(sourceIndex);
            }
        }
    });

    isConnected = true;
}

void cwUniqueValueFilterModel::disconnectSourceModel()
{
    if(this->sourceModel()) {
        qDebug() << "Disconnecting source model:" << this->sourceModel();
        disconnect(this->sourceModel(), nullptr, this, nullptr);
        isConnected = false;
    }
}

void cwUniqueValueFilterModel::setUniqueRole(int uniqueRole) {
    if(mUniqueRole != uniqueRole) {
        mUniqueRole = uniqueRole;
        emit uniqueRoleChanged();
        buildIndex();
    }
}

bool cwUniqueValueFilterModel::contains(const QVariant &key) const
{
    auto iter = findProxyIndex(key);
    return iter != mUniqueIndex.end() && iter->value == key;
}
