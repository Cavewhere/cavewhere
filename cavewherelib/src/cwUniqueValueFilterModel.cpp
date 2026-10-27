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
    const int row = findRow(uniqueValue(sourceIndex));
    if(row < 0) {
        return QModelIndex();
    }
    return index(row);
}

void cwUniqueValueFilterModel::buildIndex()
{
    if(!mUniqueIndex.isEmpty() || isValid()) {
        beginResetModel();
        if(isValid()) {
            if(!isConnected) {
                connectSourceModel();
            }
        } else {
            disconnectSourceModel();
        }
        rebuildRows();
        endResetModel();
    }
}

void cwUniqueValueFilterModel::rebuildRows()
{
    mUniqueIndex.clear();
    mSortedRows.clear();
    if(!isValid()) {
        return;
    }

    for(int i = 0; i < sourceModel()->rowCount(); i++) {
        const auto index = sourceModel()->index(i, 0);
        const QVariant value = uniqueValue(index);
        if(findRow(value) < 0) {
            appendRow({index, value});
        }
    }
}

QVector<int>::const_iterator cwUniqueValueFilterModel::lowerBound(const QVariant &value) const
{
    return std::lower_bound(mSortedRows.cbegin(),
                            mSortedRows.cend(),
                            value,
                            [this](int row, const QVariant& value)
    {
        return mLessThan(mUniqueIndex.at(row).value, value);
    });
}

int cwUniqueValueFilterModel::findRow(const QVariant &value) const
{
    auto iter = lowerBound(value);
    if(iter != mSortedRows.cend() && mUniqueIndex.at(*iter).value == value) {
        return *iter;
    }
    return -1;
}

void cwUniqueValueFilterModel::appendRow(const Row &row)
{
    const int rowNumber = static_cast<int>(mUniqueIndex.size());
    mUniqueIndex.append(row);
    // Inserted after mUniqueIndex grows, so the comparator can read the new row.
    auto iter = lowerBound(row.value);
    mSortedRows.insert(std::distance(mSortedRows.cbegin(), iter), rowNumber);
}

void cwUniqueValueFilterModel::eraseRow(int row)
{
    auto iter = lowerBound(mUniqueIndex.at(row).value);
    Q_ASSERT(iter != mSortedRows.cend() && *iter == row);
    mSortedRows.removeAt(std::distance(mSortedRows.cbegin(), iter));
    for(int& sortedRow : mSortedRows) {
        if(sortedRow > row) {
            --sortedRow;
        }
    }
    mUniqueIndex.removeAt(row);
}

/**
 * Appends one row for each new value among the source rows first..last, with
 * one insert over the new range.
 */
void cwUniqueValueFilterModel::insertSourceRows(int first, int last)
{
    QVector<Row> pending;
    QVector<int> pendingSorted;
    for(int i = first; i <= last; i++) {
        const auto index = sourceModel()->index(i, 0);
        Q_ASSERT(index.isValid());
        const QVariant value = uniqueValue(index);
        if(findRow(value) >= 0) {
            continue;
        }

        auto iter = std::lower_bound(pendingSorted.cbegin(), pendingSorted.cend(), value,
                                     [this, &pending](int row, const QVariant& value)
        {
            return mLessThan(pending.at(row).value, value);
        });
        if(iter != pendingSorted.cend() && pending.at(*iter).value == value) {
            continue;
        }
        pendingSorted.insert(std::distance(pendingSorted.cbegin(), iter), static_cast<int>(pending.size()));
        pending.append({index, value});
    }

    if(pending.isEmpty()) {
        return;
    }

    const int firstRow = static_cast<int>(mUniqueIndex.size());
    beginInsertRows(QModelIndex(), firstRow, firstRow + static_cast<int>(pending.size()) - 1);
    for(const Row& row : std::as_const(pending)) {
        appendRow(row);
    }
    endInsertRows();
}

/**
 * sourceIndex stops carrying the value of row. If sourceIndex represents that
 * value, another source row with it outside skipFirst..skipLast takes its
 * place, or the row goes away.
 */
void cwUniqueValueFilterModel::releaseRow(int row, const QModelIndex &sourceIndex, int skipFirst, int skipLast)
{
    if(mUniqueIndex.at(row).index != sourceIndex) {
        return;
    }

    const QVariant value = mUniqueIndex.at(row).value;
    const int n = sourceModel()->rowCount();
    for(int i = 0; i < n; i++) {
        if(i >= skipFirst && i <= skipLast) {
            continue;
        }
        const auto candidate = sourceModel()->index(i, 0);
        if(candidate != sourceIndex && uniqueValue(candidate) == value) {
            mUniqueIndex[row].index = candidate;
            return;
        }
    }

    beginRemoveRows(QModelIndex(), row, row);
    eraseRow(row);
    endRemoveRows();
}

void cwUniqueValueFilterModel::connectSourceModel()
{
    if(!sourceModel()) {
        return;
    }

    connect(sourceModel(), &QAbstractItemModel::rowsInserted,
            this, [this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent);

        //This is a work around to the row inconsistancy in sourceModel()
        if(sourceModel()->columnCount() <= 0) {
            return;
        }

        insertSourceRows(first, last);
    });

    //This is to handle concatination model inconsistancy with column rows
    connect(sourceModel(), &QAbstractItemModel::columnsInserted,
            this, [this](const QModelIndex& parent, int first, int last) {
        Q_UNUSED(parent);
        Q_UNUSED(first);
        Q_UNUSED(last);

        if(sourceModel()->rowCount() <= 0) {
            return;
        }

        if(mUniqueIndex.isEmpty()) {
            //Only run on new models
            insertSourceRows(0, sourceModel()->rowCount() - 1);
        }
    });

    connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved,
            this, [this](const QModelIndex& parent, int first, int last)
    {
        Q_ASSERT(sourceModel() == sender()); //if this fails, we haven't disconnected a old sourceModel()
        Q_UNUSED(parent);
        for(int i = first; i <= last; i++) {
            const auto sourceIndex = this->sourceModel()->index(i, 0);
            const int row = findRow(uniqueValue(sourceIndex));
            if(row >= 0) {
                releaseRow(row, sourceIndex, first, last);
            }
        }
    });

    connect(sourceModel(), &QAbstractItemModel::modelAboutToBeReset,
            this, [this]()
    {
        beginResetModel();
    });

    connect(sourceModel(), &QAbstractItemModel::modelReset,
            this, [this]()
    {
        rebuildRows();
        endResetModel();
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
            this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int> roles)
    {
        if(roles.contains(mUniqueRole)) {
            for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
                const auto sourceIndex = this->sourceModel()->index(i, 0);

                //Did we index the old value
                auto iter = std::find_if(mUniqueIndex.cbegin(), mUniqueIndex.cend(),
                                         [sourceIndex](const Row& row)
                {
                    return row.index == sourceIndex;
                });

                if(iter != mUniqueIndex.cend()) {
                    //Indexed under its old value: hand that value to another
                    //row or drop it, then index the new value.
                    releaseRow(static_cast<int>(std::distance(mUniqueIndex.cbegin(), iter)), sourceIndex, i, i);
                }

                insertSourceRows(i, i);
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

bool cwUniqueValueFilterModel::contains(const QVariant &key) const
{
    return findRow(key) >= 0;
}
