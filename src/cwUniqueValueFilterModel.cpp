#include "cwUniqueValueFilterModel.h"

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
    if(index.isValid()) {
        return mUniqueIndex.at(index.row()).index.data(role);
    }
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
        mUniqueIndex.insert(iter, toRow(sourceIndex));
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
    //Use the non-const version
    return const_cast<QVector<Row>::const_iterator>(const_cast<cwUniqueValueFilterModel*>(this)->findProxyIndex(value));
}

QVector<cwUniqueValueFilterModel::Row>::iterator cwUniqueValueFilterModel::findProxyIndex(const QVariant &value)
{
    return std::lower_bound(mUniqueIndex.begin(),
                            mUniqueIndex.end(),
                            value,
                            [this](const Row& row, const QVariant& value)
    {
        Q_ASSERT(row.index.isValid());
        return mLessThan(uniqueValue(row.index), value);
    });
}

void cwUniqueValueFilterModel::connectSourceModel()
{
    auto insert = [this](const QModelIndex& index) {
        this->findRunAction<void>(index,
                            [this, index](auto iter)
        {
            return !contains(iter, index);
        },

        [this, index](auto iter)
        {
            int row = iteratorToRow(iter);
            beginInsertRows(QModelIndex(), row, row);
            qDebug() << "Insert:" << toRow(index).value;
            mUniqueIndex.insert(iter, toRow(index));
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

        for(int i = first; i <= last; i++) {
            const auto index = this->sourceModel()->index(i, 0);
            insert(index);
        }
    });

    connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved,
            this, [this, remove](const QModelIndex& parent, int first, int last)
    {
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

    connect(sourceModel(), &QAbstractItemModel::dataChanged,
            this, [this, insert, remove](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int> roles)
    {
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

void cwUniqueValueFilterModel::setLessThan(std::function<bool (const QVariant &, const QVariant &)> lessThan) {
    mLessThan = lessThan;
    buildIndex();
}

bool cwUniqueValueFilterModel::contains(const QVariant &key) const
{
    auto iter = findProxyIndex(key);
    return iter != mUniqueIndex.end();
}
