//Our includes
#include "cwKeywordGroupByKeyModel.h"
#include "cwKeyword.h"
#include "cwAsyncFuture.h"
#include "cwGlobals.h"
#include "cwKeywordModel.h"
#include "cwKeywordFilterModel.h"

//Async includes
#include "asyncfuture.h"

//Qt includes
#include <QtConcurrent>
#include <QQuickItem>
#include <QEntity>

cwKeywordGroupByKeyModel::cwKeywordGroupByKeyModel(QObject *parent) :
    QAbstractListModel(parent),
    mAcceptedModel(new cwKeywordFilterModel(this))
{
    mFilter = createDefaultFilter();
}

cwKeywordGroupByKeyModel::~cwKeywordGroupByKeyModel()
{
}

int cwKeywordGroupByKeyModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return mData.size();
}

QVariant cwKeywordGroupByKeyModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    auto row = mData.row(index.row());

    switch(role) {
    case ObjectsRole: {
        QVector<QObject*> objects;
        objects.reserve(row.indexes.size());
        for(auto index : row.indexes) {
            objects.append(index.data(cwKeywordItemModel::ObjectRole).value<QObject*>());
        }
        return QVariant::fromValue(objects);
    }
    case ObjectCountRole:
        return row.indexes.size();
    case ValueRole:
        return row.value;
    case AcceptedRole:
        return row.accepted;
    }

    return QVariant();
}

QHash<int, QByteArray> cwKeywordGroupByKeyModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {ObjectsRole, "objectsRole" },
        {ObjectCountRole, "objectCountRole"},
        {ValueRole, "valueRole"},
        {AcceptedRole, "filterRole"}
    };
    return roles;
}

QString cwKeywordGroupByKeyModel::otherCategory()
{
    return QStringLiteral("Others");
}

QModelIndex cwKeywordGroupByKeyModel::otherIndex() const
{
    return index(rowCount() - 1);
}

/**
 * Static function that apply visiblity to QQuickItems and QEntity
 */
void cwKeywordGroupByKeyModel::setVisiblityProperty(QObject *object, bool accepted)
{
    if(auto item = dynamic_cast<QQuickItem*>(object)) {
        item->setVisible(accepted);
    } else if(auto entity = dynamic_cast<Qt3DCore::QEntity*>(object)) {
        item->setEnabled(accepted);
    }
}

void cwKeywordGroupByKeyModel::updateAllRows()
{
    beginResetModel();
    mData.clear();

    //Use an empty filter to prevent signals from being emitting
    Filter filter;
    filter.data = &mData;
    filter.key = mKey;

    for(int i = 0; i < mSourceModel->rowCount(); i++) {
        auto index = mSourceModel->index(i, 0, QModelIndex());
        filter.filterEntity(index);
    }

    endResetModel();
}

cwKeywordGroupByKeyModel::Filter cwKeywordGroupByKeyModel::createDefaultFilter()
{
    auto beginInsertFunction = [this](int i) {
        beginInsertRows(QModelIndex(), i, i);
    };

    auto endInsertFunction = [this]() {
        endInsertRows();
    };

    auto beginRemoveFunction = [this](int i) {
        beginRemoveRows(QModelIndex(), i, i);
    };

    auto endRemoveFunction = [this]() {
        endRemoveRows();
    };

    auto dataChangeFunction = [this](int i, QVector<int> roles) {
        auto modelIndex = index(i);
        emit dataChanged(modelIndex, modelIndex, roles);
    };

    Filter insertRow;
    insertRow.key = mKey;
    insertRow.data = &mData;
    insertRow.beginRemoveFuction = beginRemoveFunction;
    insertRow.endRemoveFunction = endRemoveFunction;
    insertRow.beginInsertFunction = beginInsertFunction;
    insertRow.endInsertFunction = endInsertFunction;
    insertRow.dataChangedFunction = dataChangeFunction;

    return insertRow;
}



/**
*
*/
void cwKeywordGroupByKeyModel::setKey(QString keyLast) {
    if(mKey != keyLast) {
        mKey = keyLast;
        updateAllRows();
        emit keyChanged();
    }
}

/**
 * Will invert all the accepted
 */
void cwKeywordGroupByKeyModel::invert()
{
    if(rowCount() > 0) {
        for(int i = 0; i < rowCount(); i++) {
            auto& row = mData.row(i);
            setAccepted(index(i), !row.accepted, nullptr);
        }

        emit dataChanged(index(0), index(rowCount() - 1), {AcceptedRole});
    }
}

void cwKeywordGroupByKeyModel::Filter::addEntity(const QString &value,
                                                 const QModelIndex& sourceIndex)
{
    auto& rows = data->Rows;
    auto iter = std::lower_bound(rows.begin(), rows.end(), value, &lessThan);

    if(iter != rows.end() && iter->value == value) {
        if(!iter->indexes.contains(sourceIndex)) {
            //Add to an existing row, dataChanged
            iter->indexes.append(sourceIndex);
            dataChanged(iter);
        }
    } else {
        beginInsert(iter);
        rows.insert(iter, Row(value, {sourceIndex}, true));
        endInsert();
    }

    bool removed = data->OtherRow.indexes.removeOne(sourceIndex);
    if(removed) {
        dataChanged(data->otherRowIndex());
    }
}

void cwKeywordGroupByKeyModel::Filter::addEntityToOther(const QModelIndex& sourceIndex)
{
    data->OtherRow.indexes.append(sourceIndex);
    dataChanged(data->otherRowIndex());
}

void cwKeywordGroupByKeyModel::Filter::removeEntity(const QString &value, const QModelIndex& sourceIndex)
{
    auto& rows = data->Rows;
    auto iter = std::lower_bound(rows.begin(), rows.end(), value, &lessThan);

    if(iter != rows.end() && iter->value == value) {
        bool removed = removeEntityFromRow(iter, sourceIndex);
        if(removed) {
            bool stillContainsEntity = false;
            for(auto row : rows) {
                if(row.indexes.contains(sourceIndex)) {
                    stillContainsEntity = true;
                    break;
                }
            }

            if(!stillContainsEntity) {
                addEntityToOther(sourceIndex);
            }
        }
    } else {
        addEntityToOther(sourceIndex);
    }
}

bool cwKeywordGroupByKeyModel::Filter::removeEntityFromRow(QVector<Row>::iterator iter, const QModelIndex& sourceIndex)
{
    return removeEntityFromRow(std::distance(data->Rows.begin(), iter), *iter, sourceIndex);
}

bool cwKeywordGroupByKeyModel::Filter::removeEntityFromRow(int i, Row &row, const QModelIndex& sourceIndex)
{
    bool removed = row.indexes.removeOne(sourceIndex);
    if(removed) {
        if(row.indexes.isEmpty()) {
            //No more entities
            beginRemoveFuction(i);
            data->Rows.removeAt(i);
            endRemoveFunction();
        } else {
            dataChanged(i);
        }
    }
    return removed;
}

void cwKeywordGroupByKeyModel::Filter::beginInsert(const QVector<Row>::iterator &iter) const
{
    if(beginInsertFunction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginInsertFunction(index);
    }
}

void cwKeywordGroupByKeyModel::Filter::endInsert() const
{
    if(endInsertFunction) {
        endInsertFunction();
    }
}

void cwKeywordGroupByKeyModel::Filter::beginRemove(const QVector<Row>::iterator &iter) const
{
    if(beginRemoveFuction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginRemoveFuction(index);
    }
}

void cwKeywordGroupByKeyModel::Filter::endRemove() const
{
    if(endRemoveFunction) {
        endRemoveFunction();
    }
}

void cwKeywordGroupByKeyModel::Filter::dataChanged(const QVector<Row>::iterator &iter) const
{
    int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
    dataChanged(index);
}

void cwKeywordGroupByKeyModel::Filter::dataChanged(int index) const
{
    if(dataChangedFunction) {
        dataChangedFunction(index, {ObjectsRole, ObjectCountRole});
    }
}

bool cwKeywordGroupByKeyModel::Filter::lessThan(const cwKeywordGroupByKeyModel::Row &row, const QString &value)
{
    return row.value < value;
}

void cwKeywordGroupByKeyModel::Filter::filterEntity(const QModelIndex& sourceIndex)
{
    auto keywords = sourceIndex.data(cwKeywordItemModel::KeywordsRole).value<QVector<cwKeyword>>();

    addEntityToOther(sourceIndex);

    //Existing values for the entity
    QSet<QString> oldValues = entityValues(sourceIndex);

    for(const auto& keyword : qAsConst(keywords)) {
        if(keyword.key() == key) {
            if(!oldValues.contains(keyword.value())) {
                addEntity(keyword.value(), sourceIndex);
            } else {
                //Doesn't have to be removed
                oldValues.remove(keyword.value());
            }
        }
    }

    for(const auto& valueToRemove : oldValues) {
        removeEntity(valueToRemove, sourceIndex);
    }
}

void cwKeywordGroupByKeyModel::Filter::removeEntityFromAllRows(const QModelIndex& sourceIndex)
{
    for(auto iter = data->Rows.begin(); iter < data->Rows.end(); iter++) {
        removeEntityFromRow(iter, sourceIndex);
    }
    removeEntityFromRow(data->otherRowIndex(), data->OtherRow, sourceIndex);
}

QSet<QString> cwKeywordGroupByKeyModel::Filter::entityValues(const QModelIndex& sourceIndex) const
{
    QSet<QString> valuesOfEntity;
    for(const auto& row : data->Rows) {
        if(row.indexes.contains(sourceIndex)) {
            valuesOfEntity.insert(row.value);
        }
    }
    return valuesOfEntity;
}

bool cwKeywordGroupByKeyModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(index.isValid() && role == AcceptedRole) {
        setAccepted(index, value.toBool(), [this, index]() {
            emit dataChanged(index, index, {AcceptedRole});
        });
        return true;
    }
    return false;
}

void cwKeywordGroupByKeyModel::setSourceModel(QAbstractItemModel* source) {
    //Source must be a cwKeywordItemModel or cwKeywordFilterModel
    Q_ASSERT(dynamic_cast<cwKeywordItemModel*>(source)
             || dynamic_cast<cwKeywordFilterModel*>(source)
             || source == nullptr);

    if(mSourceModel != source) {
        if(mSourceModel) {
            disconnect(mSourceModel, nullptr, this, nullptr);
        }

        mSourceModel = source;

        if(mSourceModel) {

            auto toIndex = [this](int rowIndex) {
                return mSourceModel->index(rowIndex, 0, QModelIndex());
            };

            auto updateEntity = [this, toIndex](int sourceRow) {
                mFilter.filterEntity(toIndex(sourceRow));
            };

            connect(mSourceModel, &QAbstractItemModel::dataChanged,
                    this, [updateEntity](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
            {
                if(roles.contains(cwKeywordItemModel::KeywordsRole)) {
                    for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
                        updateEntity(i);
                    }
                }
            });

            connect(mSourceModel, &QAbstractItemModel::rowsInserted,
                    this, [updateEntity](const QModelIndex& parent, int begin, int last)
            {
                if(parent == QModelIndex()) {
                    for(int i = begin; i <= last; i++) {
                        updateEntity(i);
                    }
                }
            });

            connect(mSourceModel, &QAbstractItemModel::rowsRemoved,
                    this, [this, toIndex](const QModelIndex& parent, int begin, int last)
            {
                if(parent == QModelIndex()) {
                    for(int i = begin; i <= last; i++) {
                        mFilter.removeEntityFromAllRows(toIndex(i));
                    }
                }
            });

            updateAllRows();
        }

        mAcceptedModel->setSourceModel(mSourceModel);

        emit sourceChanged();
    }
}
