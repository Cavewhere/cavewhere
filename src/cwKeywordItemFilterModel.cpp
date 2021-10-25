//Our includes
#include "cwKeywordItemFilterModel.h"
#include "cwKeyword.h"
#include "cwAsyncFuture.h"

//Async includes
#include "asyncfuture.h"

//Qt includes
#include <QtConcurrent>

cwKeywordItemFilterModel::cwKeywordItemFilterModel(QObject *parent) :
    QAbstractListModel(parent)
{

}

cwKeywordItemFilterModel::~cwKeywordItemFilterModel()
{
    waitForFinished();
}

/**
*
*/
void cwKeywordItemFilterModel::setKeywordModel(cwKeywordItemModel* keywordModel) {
    if(KeywordModel != keywordModel) {

        if(KeywordModel) {
            disconnect(KeywordModel, nullptr, this, nullptr);
        }

        KeywordModel = keywordModel;

        if(KeywordModel) {

            auto createPairs = [this](const QModelIndex& parent, int begin, int end){
                QVector<EntityAndKeywords> pairs;

                if(parent == QModelIndex()) {
                    for(int i = begin; i <= end; i++) {
                        auto entityIndex = KeywordModel->index(i, 0, QModelIndex());
                        pairs.append(EntityAndKeywords(entityIndex));
                    }
                } else {
                    pairs.append(EntityAndKeywords(parent));
                }

                return pairs;
            };

            connect(KeywordModel, &cwKeywordItemModel::dataChanged,
                    this, [=](const QModelIndex& begin, const QModelIndex& end, const QVector<int>& roles)
            {
                Q_UNUSED(end)
                Q_UNUSED(roles)

                if(isRunning()) {
                    updateAllRows();
                    return;
                }
                Q_ASSERT(begin.parent() != QModelIndex()); //Only support value or key data updates

                auto entityIndex = begin.parent();
                EntityAndKeywords pair(entityIndex);

                auto filter = createDefaultFilter();
                filter.filterEntity(pair);
            });

            connect(KeywordModel, &cwKeywordItemModel::rowsInserted,
                    this, [this, createPairs](const QModelIndex& parent, int begin, int end)
            {
                //FIXME: Needs to update possible keys if a new key is added
//                if(isRunning()) {
                    updateAllRows();
                    return;
//                }

                auto pairs = createPairs(parent, begin, end);

                auto filter = createDefaultFilter();
                for(auto pair : pairs) {
                    filter.filterEntity(pair);
                }
            });

            connect(KeywordModel, &cwKeywordItemModel::rowsAboutToBeRemoved,
                    this, [this, createPairs](const QModelIndex& parent, int begin, int end)
            {
                if(isRunning()) {
                    updateAllRows();
                    return;
                }

                auto removePairs = createPairs(parent, begin, end);
                auto filter = createDefaultFilter();

                auto removeKeywords = [&filter](const QVector<cwKeyword>::iterator& begin,
                        const QVector<cwKeyword>::iterator& end,
                        QObject* entity
                        )
                {
                    for(auto iter = begin; iter != end; iter++) {
                        filter.removeEntity(iter->value(), entity);
                    }
                };

                if(parent == QModelIndex()) {
                    //Entities removed
                    for(auto pair : removePairs) {
                        filter.dataChangedFunction = nullptr; //Ignore data changes, because we're removing a whole entity
                        removeKeywords(pair.keywords.begin(), pair.keywords.end(), pair.entity);
                    }
                } else {
                    Q_ASSERT(removePairs.size() == 1);
                    auto& pair = removePairs.first();

                    //If pair is currently in the filterModel
                    auto beginIter = pair.keywords.begin() + begin;
                    auto endIter = pair.keywords.begin() + end + 1;
                    removeKeywords(beginIter, endIter, pair.entity);
                }
            });
        }

        updateAllRows();

        emit keywordModelChanged();
    }
}

int cwKeywordItemFilterModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return Data.size();
}

QVariant cwKeywordItemFilterModel::data(const QModelIndex &index, int role) const
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
    }

    return QVariant();
}

QHash<int, QByteArray> cwKeywordItemFilterModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {ObjectsRole, "objectsRole" },
        {ObjectCountRole, "objectCountRole"},
        {ValueRole, "valueRole"},
    };
    return roles;
}

QString cwKeywordItemFilterModel::otherCategory()
{
    return QStringLiteral("Other");
}

void cwKeywordItemFilterModel::waitForFinished()
{
    cwAsyncFuture::waitForFinished(LastRun);
    Q_ASSERT(isRunning() == false);
}

void cwKeywordItemFilterModel::updateAllRows()
{
    qDebug() << "Update rows";

    int entityCount = keywordModel()->rowCount(QModelIndex());

    QVector<EntityAndKeywords> entities;
    entities.reserve(entityCount);

    for(int i = 0; i < entityCount; i++) {
        auto entityIndex = keywordModel()->index(i, 0, QModelIndex());
        EntityAndKeywords pair(entityIndex);
        entities.append(pair);
    }

    auto keywords = this->keywords();
    auto lastKey = this->lastKey();

    //This method does all the filtering of the rows in the model
    auto filterRows = [keywords, lastKey, entities]() {
        ModelData newModelData;

        Filter insertRow;
        insertRow.keywords = keywords;
        insertRow.lastKey = lastKey;
        insertRow.data = &newModelData;

        QSet<QString> possibleKeys;
        auto addKeys = [&possibleKeys](const EntityAndKeywords& objectKeywords) {
            for(auto keyword : objectKeywords.keywords) {
                possibleKeys.insert(keyword.key());
            }
        };

        auto removeKeys = [&possibleKeys](QList<cwKeyword> keywords) {
            for(auto keyword : keywords) {
                possibleKeys.remove(keyword.key());
            }
        };

        for(auto entity : entities) {
            insertRow.filterEntity(entity);
            addKeys(entity);
        }

        removeKeys(keywords);
        newModelData.PossibleKeys = possibleKeys.toList();
        std::sort(newModelData.PossibleKeys.begin(), newModelData.PossibleKeys.end());

        return newModelData;
    };

    if(isRunning()) {
        LastRun.cancel();
        LastModelData.cancel();
    }

    //Run the job on the threadpool
    auto runFuture = QtConcurrent::run(filterRows);
    LastModelData = runFuture;

    LastRun = AsyncFuture::observe(runFuture).subscribe([=]()
    {
        //Updated the model will new filtered results
        beginResetModel();
        auto oldPossibleKeys = Data.PossibleKeys;
        Data = runFuture.result();
        endResetModel();

        if(oldPossibleKeys != Data.PossibleKeys) {
            qDebug() << "Possible keys:" << Data.PossibleKeys;
            emit possibleKeysChanged();
        }
    }
    ).future();
}

bool cwKeywordItemFilterModel::isRunning() const
{
    return LastModelData.isRunning();
}

cwKeywordItemFilterModel::Filter cwKeywordItemFilterModel::createDefaultFilter()
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
    insertRow.keywords = Keywords;
    insertRow.lastKey = LastKey;
    insertRow.data = &Data;
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
void cwKeywordItemFilterModel::setKeywords(QList<cwKeyword> keywords) {
    if(Keywords != keywords) {
        Keywords = keywords;
        updateAllRows();
        emit keywordsChanged();
    }
}

/**
*
*/
void cwKeywordItemFilterModel::setLastKey(QString keyLast) {
    if(LastKey != keyLast) {
        LastKey = keyLast;
        updateAllRows();
        emit keyLastChanged();
    }
}

void cwKeywordItemFilterModel::Filter::addEntity(const QString &value,
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
        rows.insert(iter, Row(value, {entity}));
        endInsert();
    }

    bool removed = data->OtherRow.entities.removeOne(entity);
    if(removed) {
        dataChanged(data->otherRowIndex());
    }
}

void cwKeywordItemFilterModel::Filter::addEntityToOther(QObject *entity)
{
    data->OtherRow.entities.append(entity);
    dataChanged(data->otherRowIndex());
}

void cwKeywordItemFilterModel::Filter::removeEntity(const QString &value, QObject *entity)
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

void cwKeywordItemFilterModel::Filter::beginInsert(const QVector<Row>::iterator &iter) const
{
    if(beginInsertFunction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginInsertFunction(index);
    }
}

void cwKeywordItemFilterModel::Filter::endInsert() const
{
    if(endInsertFunction) {
        endInsertFunction();
    }
}

void cwKeywordItemFilterModel::Filter::beginRemove(const QVector<Row>::iterator &iter) const
{
    if(beginRemoveFuction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginRemoveFuction(index);
    }
}

void cwKeywordItemFilterModel::Filter::endRemove() const
{
    if(endRemoveFunction) {
        endRemoveFunction();
    }
}

void cwKeywordItemFilterModel::Filter::dataChanged(const QVector<Row>::iterator &iter) const
{
    int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
    dataChanged(index);
}

void cwKeywordItemFilterModel::Filter::dataChanged(int index) const
{
    if(dataChangedFunction) {
        dataChangedFunction(index, {ObjectsRole, ObjectCountRole});
    }
}

bool cwKeywordItemFilterModel::Filter::lessThan(const cwKeywordItemFilterModel::Row &row, const QString &value)
{
    return row.value < value;
}

void cwKeywordItemFilterModel::Filter::filterEntity(const cwKeywordItemFilterModel::EntityAndKeywords &pair)
{

    if(keywords.isEmpty()) {
        addEntityToOther(pair.entity);
    }

    for(auto keyword : keywords) {
        if(!pair.keywords.contains(keyword)) {
            //Missing one of the keywords
            removeEntity(keyword.value(), pair.entity);
            return;
        }
    }

    //Existing values for the entity
    QSet<QString> oldValues = entityValues(pair.entity);

    for(auto keyword : pair.keywords) {
        if(keyword.key() == lastKey) {
            if(!oldValues.contains(keyword.value())) {
                addEntity(keyword.value(), pair.entity);
            } else {
                oldValues.remove(keyword.value());
            }
        }
    }

    for(auto valueToRemove : oldValues) {
        removeEntity(valueToRemove, pair.entity);
    }
}

QSet<QString> cwKeywordItemFilterModel::Filter::entityValues(QObject *entity) const
{
    QSet<QString> valuesOfEntity;
    for(auto row : data->Rows) {
        if(row.entities.contains(entity)) {
            valuesOfEntity.insert(row.value);
        }
    }
    return valuesOfEntity;
}

cwKeywordItemFilterModel::EntityAndKeywords::EntityAndKeywords(const QModelIndex &entityIndex)
{
    Q_ASSERT(entityIndex.parent() == QModelIndex());
    entity = entityIndex.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
    keywords = entityIndex.data(cwKeywordItemModel::KeywordsRole).value<QVector<cwKeyword>>();
}
