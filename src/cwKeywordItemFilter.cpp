//Our includes
#include "cwKeywordItemFilter.h"
#include "cwKeyword.h"
#include "cwAsyncFuture.h"
#include "cwGlobals.h"
#include "cwKeywordModel.h"

//Async includes
#include "asyncfuture.h"

//Qt includes
#include <QtConcurrent>

cwKeywordItemFilter::cwKeywordItemFilter(QObject *parent) :
    QAbstractListModel(parent)
//    mFilterKeywords(new cwKeywordModel(this))
{
//    connect(mFilterKeywords, &cwKeywordModel::rowsInserted,
//            this, [this](const QModelIndex& parent, int first, int last)
//    {
//        Q_UNUSED(parent);
//        Q_UNUSED(first);
//        Q_UNUSED(last);
//        updateAllRows();
//    });

//    connect(mFilterKeywords, &cwKeywordModel::rowsRemoved,
//            this, [this](const QModelIndex& parent, int first, int last)
//    {
//        Q_UNUSED(parent);
//        Q_UNUSED(first);
//        Q_UNUSED(last);
//        updateAllRows();
//    });

//    connect(mFilterKeywords, &cwKeywordModel::dataChanged,
//            this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int> roles)
//    {
//        Q_UNUSED(topLeft);
//        Q_UNUSED(bottomRight);
//        Q_UNUSED(roles);
//        updateAllRows();
//    });

    mFilter = createDefaultFilter();
}

cwKeywordItemFilter::~cwKeywordItemFilter()
{
//    waitForFinished();
}

void cwKeywordItemFilter::setEntities(QVector<cwEntityAndKeywords> entries) {
//    if(mEntries != entries) {
        mEntries = entries;
        emit entitiesChanged();
        updateAllRows();
//    }
}

int cwKeywordItemFilter::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return Data.size();
}

QVariant cwKeywordItemFilter::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    auto row = Data.row(index.row());

    switch(role) {
    case ObjectsRole:
        return QVariant::fromValue(row.entities);
    case ObjectCountRole:
        return row.entities.size();
    case ValueRole:
        return row.value;
    case AcceptedRole:
        return row.accepted;
    }

    return QVariant();
}

QHash<int, QByteArray> cwKeywordItemFilter::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {ObjectsRole, "objectsRole" },
        {ObjectCountRole, "objectCountRole"},
        {ValueRole, "valueRole"},
        {AcceptedRole, "filterRole"}
    };
    return roles;
}

QString cwKeywordItemFilter::otherCategory()
{
    return QStringLiteral("Others");
}

QModelIndex cwKeywordItemFilter::otherIndex() const
{
    return index(rowCount() - 1);
}

void cwKeywordItemFilter::updateFilterEntries()
{


}

//void cwKeywordItemFilter::waitForFinished()
//{
////    cwAsyncFuture::waitForFinished(mEntries);
////    cwAsyncFuture::waitForFinished(mFilterEntries);
////    cwAsyncFuture::waitForFinished(mLastModelData);
//    Q_ASSERT(isRunning() == false);
//}

//QVector<cwKeywordItemFilter::EntityAndKeywords> cwKeywordItemFilter::entities() const {
//    int entityCount = keywordModel()->rowCount(QModelIndex());

//    QVector<EntityAndKeywords> entities;
//    entities.reserve(entityCount);

//    for(int i = 0; i < entityCount; i++) {
//        auto entityIndex = keywordModel()->index(i, 0, QModelIndex());
//        EntityAndKeywords pair(entityIndex);
//        entities.append(pair);
//    }

//    return entities;
//}

void cwKeywordItemFilter::updateAllRows()
{


    auto entities = this->entities();
//    auto keywords = this->filterKeywords()->keywords();
    auto lastKey = this->key();

    if(lastKey.isEmpty()) {
        Data.OtherRow.entities = cwEntityAndKeywords::justEnitites(entities);
        emit dataChanged(otherIndex(), otherIndex(), {ObjectsRole, ObjectCountRole});
        return;
    }

    //This method does all the filtering of the rows in the model
//    auto rows = [this, entities]() {
//        ModelData newModelData;

//        Filter insertRow;
//        insertRow.keywords = keywords;
//        insertRow.lastKey = lastKey;
//        insertRow.data = &newModelData;
        mFilter.lastKey = this->key();

        for(const auto& entity : qAsConst(entities)) {
            mFilter.filterEntity(entity);
        }

        emit acceptedEntitesChanged();

//        newModelData.PossibleKeys = cw::toList(createPossibleKeys(keywords, entities));
//        std::sort(newModelData.PossibleKeys.begin(), newModelData.PossibleKeys.end());

//        return newModelData;
//    };



//    if(isRunning()) {
//        LastRun.cancel();
//        LastModelData.cancel();
//    }

//    //Run the job on the threadpool
//    auto runFuture = QtConcurrent::run(rows);
//    LastModelData = runFuture;

//    LastRun = AsyncFuture::observe(runFuture).subscribe([=]()
//    {
//        //Updated the model will new filtered results
//        beginResetModel();
//        auto oldPossibleKeys = Data.PossibleKeys;
//        Data = runFuture.result();
//        endResetModel();

//        if(oldPossibleKeys != Data.PossibleKeys) {
//            emit possibleKeysChanged();
//        }
//    }
//    ).future();
}

//bool cwKeywordItemFilter::isRunning() const
//{
//    return mEntries.isRunning() && mFilterEntries.isRunning() && mLastModelData.isRunning();
//}

cwKeywordItemFilter::Filter cwKeywordItemFilter::createDefaultFilter()
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
//    insertRow.keywords = mFilterKeywords->keywords();
    insertRow.lastKey = mKey;
    insertRow.data = &Data;
    insertRow.beginRemoveFuction = beginRemoveFunction;
    insertRow.endRemoveFunction = endRemoveFunction;
    insertRow.beginInsertFunction = beginInsertFunction;
    insertRow.endInsertFunction = endInsertFunction;
    insertRow.dataChangedFunction = dataChangeFunction;

    return insertRow;
}

//QSet<QString> cwKeywordItemFilter::createPossibleKeys(const QVector<cwKeyword> &keywords,
//                                                           const QVector<EntityAndKeywords> entities)
//{
//    QSet<QString> possibleKeys;
//    auto addKeys = [&possibleKeys](const EntityAndKeywords& objectKeywords) {
//        for(const auto& keyword : qAsConst(objectKeywords.keywords)) {
//            possibleKeys.insert(keyword.key());
//        }
//    };

//    auto removeKeys = [&possibleKeys](const QVector<cwKeyword>& keywords) {
//        for(const auto& keyword : qAsConst(keywords)) {
//            possibleKeys.remove(keyword.key());
//        }
//    };

//    for(const auto& entity : qAsConst(entities)) {
//        addKeys(entity);
//    }

//    removeKeys(keywords);
//    return possibleKeys;
//}

//void cwKeywordItemFilter::addKeywordFromLastKey(const QString& value)
//{
//    mFilterKeywords->add(cwKeyword(key(), value));
////    updateAllRows();
////    emit keywordsChanged();
//}

//void cwKeywordItemFilter::removeLastKeyword()
//{
//    if(mFilterKeywords->rowCount() > 0) {
//        //Find and remove the last keyword
//        mFilterKeywords->remove(mFilterKeywords->index(mFilterKeywords->rowCount() - 1)
//                         .data(cwKeywordModel::KeywordRole)
//                         .value<cwKeyword>());
//    }

////    updateAllRows();
////    emit keywordsChanged();
//}

/**
*
*/
void cwKeywordItemFilter::setKey(QString keyLast) {
    if(mKey != keyLast) {
        mKey = keyLast;
        updateAllRows();
        emit keyChanged();
    }
}

/**
 * Will invert all the accepted
 */
void cwKeywordItemFilter::invert()
{
    if(rowCount() > 0) {

        for(int i = 0; i < rowCount(); i++) {
            auto& row = Data.row(i);
            row.accepted = !row.accepted;
        }

        emit dataChanged(index(0), index(rowCount() - 1), {AcceptedRole});
        emit acceptedEntitesChanged();
    }
}

void cwKeywordItemFilter::Filter::addEntity(const QString &value,
                                                 QObject *entity)
{
    auto& rows = data->Rows;
    auto iter = std::lower_bound(rows.begin(), rows.end(), value, &lessThan);

    if(iter != rows.end() && iter->value == value) {
        if(!iter->entities.contains(entity)) {
            //Add to an existing row, dataChanged
            iter->entities.append(entity);
            dataChanged(iter);
        }
    } else {
        beginInsert(iter);
        rows.insert(iter, Row(value, {entity}, true));
        endInsert();
    }

    bool removed = data->OtherRow.entities.removeOne(entity);
    if(removed) {
        dataChanged(data->otherRowIndex());
    }
}

void cwKeywordItemFilter::Filter::addEntityToOther(QObject *entity)
{
    data->OtherRow.entities.append(entity);
    dataChanged(data->otherRowIndex());
}

void cwKeywordItemFilter::Filter::removeEntity(const QString &value, QObject *entity)
{
    auto& rows = data->Rows;
    auto iter = std::lower_bound(rows.begin(), rows.end(), value, &lessThan);

    if(iter != rows.end() && iter->value == value) {
        bool removed = iter->entities.removeOne(entity);
        if(removed) {
            if(iter->entities.isEmpty()) {
                //No more entities
                beginRemove(iter);
                rows.erase(iter);
                endRemove();
            } else {
                dataChanged(iter);
            }

            bool stillContainsEntity = false;
            for(auto row : rows) {
                if(row.entities.contains(entity)) {
                    stillContainsEntity = true;
                    break;
                }
            }

            if(!stillContainsEntity) {
                addEntityToOther(entity);
            }
        }
    } else {
        addEntityToOther(entity);
    }
}

void cwKeywordItemFilter::Filter::beginInsert(const QVector<Row>::iterator &iter) const
{
    if(beginInsertFunction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginInsertFunction(index);
    }
}

void cwKeywordItemFilter::Filter::endInsert() const
{
    if(endInsertFunction) {
        endInsertFunction();
    }
}

void cwKeywordItemFilter::Filter::beginRemove(const QVector<Row>::iterator &iter) const
{
    if(beginRemoveFuction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginRemoveFuction(index);
    }
}

void cwKeywordItemFilter::Filter::endRemove() const
{
    if(endRemoveFunction) {
        endRemoveFunction();
    }
}

void cwKeywordItemFilter::Filter::dataChanged(const QVector<Row>::iterator &iter) const
{
    int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
    dataChanged(index);
}

void cwKeywordItemFilter::Filter::dataChanged(int index) const
{
    if(dataChangedFunction) {
        dataChangedFunction(index, {ObjectsRole, ObjectCountRole});
    }
}

bool cwKeywordItemFilter::Filter::lessThan(const cwKeywordItemFilter::Row &row, const QString &value)
{
    return row.value < value;
}

void cwKeywordItemFilter::Filter::filterEntity(const cwEntityAndKeywords &pair)
{

//    if(keywords.isEmpty()) {
//        addEntityToOther(pair.entity);
//    }

//    for(const auto& keyword : keywords) {
//        if(!pair.keywords.contains(keyword)) {
//            //Missing one of the keywords
//            removeEntity(keyword.value(), pair.entity);
//            return;
//        }
//    }

    //Existing values for the entity
    QSet<QString> oldValues = entityValues(pair.entity());

    for(const auto& keyword : pair.keywords()) {
        if(keyword.key() == lastKey) {
            if(!oldValues.contains(keyword.value())) {
                addEntity(keyword.value(), pair.entity());
            } else {
                oldValues.remove(keyword.value());
            }
        }
    }

    for(const auto& valueToRemove : oldValues) {
        removeEntity(valueToRemove, pair.entity());
    }
}

QSet<QString> cwKeywordItemFilter::Filter::entityValues(QObject *entity) const
{
    QSet<QString> valuesOfEntity;
    for(auto row : data->Rows) {
        if(row.entities.contains(entity)) {
            valuesOfEntity.insert(row.value);
        }
    }
    return valuesOfEntity;
}

//cwKeywordItemFilter::EntityAndKeywords::EntityAndKeywords(const QModelIndex &entityIndex)
//{
//    Q_ASSERT(entityIndex.parent() == QModelIndex());
//    entity = entityIndex.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
//    keywords = entityIndex.data(cwKeywordItemModel::KeywordsRole).value<QVector<cwKeyword>>();
//}

//void cwKeywordItemFilter::setInverted(bool inverted) {
//    if(mInverted != inverted) {
//        mInverted = inverted;
//        emit invertedChanged();
//    }
//}

QVector<cwEntityAndKeywords> cwKeywordItemFilter::acceptedEntites() const {

    //Sort the mEntities for faster lookup when generating the acceptedEntities
    QVector<cwEntityAndKeywords> allEntities = mEntries;
    std::sort(allEntities.begin(), allEntities.end(),
              [](const cwEntityAndKeywords& left, const cwEntityAndKeywords& right)
    {
         return left.entity() < right.entity();
    });

    QVector<cwEntityAndKeywords> accepted;
    accepted.reserve(mEntries.size());

    //Go through all the rows
    for(int i = 0; i < rowCount(); i++) {
        auto index = this->index(i);
        bool groupAccepted = index.data(cwKeywordItemFilter::AcceptedRole).toBool();
        if(groupAccepted) {
            auto objects = index.data(cwKeywordItemFilter::ObjectsRole).value<QVector<QObject*>>();

            //For each row, get the objects that are accepted and add them to the accepted list
            for(auto object : objects) {
                auto iter = std::lower_bound(allEntities.begin(), allEntities.end(),
                                             object,
                                             [](const cwEntityAndKeywords& entityAndKeywords, const QObject* other)
                {
                    return entityAndKeywords.entity() < other;
                });
                accepted.append(*iter);
            }
        }
    }

    return accepted;
}


bool cwKeywordItemFilter::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(index.isValid() && role == AcceptedRole) {
        auto& row = Data.row(index.row());
        bool newAccepted = value.toBool();
        if(newAccepted != row.accepted) {
            row.accepted = newAccepted;
            emit dataChanged(index, index, {AcceptedRole});
            emit acceptedEntitesChanged();
        }
        return true;
    }
    return false;
}
