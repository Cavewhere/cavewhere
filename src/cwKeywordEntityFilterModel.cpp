//Our includes
#include "cwKeywordEntityFilterModel.h"
#include "cwKeyword.h"
#include "cwAsyncFuture.h"

//Async includes
#include "asyncfuture.h"

//Qt includes
#include <QtConcurrent>
using namespace Qt3DCore;

cwKeywordEntityFilterModel::cwKeywordEntityFilterModel(QObject *parent) :
    QAbstractListModel(parent)
{

}

cwKeywordEntityFilterModel::~cwKeywordEntityFilterModel()
{
    waitForFinished();
}

/**
*
*/
void cwKeywordEntityFilterModel::setKeywordModel(cwKeywordEntityModel* keywordModel) {
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

            connect(KeywordModel, &cwKeywordEntityModel::dataChanged,
                    this, [=](const QModelIndex& begin, const QModelIndex& end, const QVector<int>& roles)
            {
                Q_UNUSED(end);
                Q_UNUSED(roles);

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

            connect(KeywordModel, &cwKeywordEntityModel::rowsInserted,
                    this, [this, createPairs](const QModelIndex& parent, int begin, int end)
            {
                if(isRunning()) {
                    updateAllRows();
                    return;
                }

                auto pairs = createPairs(parent, begin, end);

                auto filter = createDefaultFilter();
                for(auto pair : pairs) {
                    filter.filterEntity(pair);
                }
            });

            connect(KeywordModel, &cwKeywordEntityModel::rowsAboutToBeRemoved,
                    this, [this, createPairs](const QModelIndex& parent, int begin, int end)
            {
                if(isRunning()) {
                    updateAllRows();
                    return;
                }

                auto pairs = createPairs(parent, begin, end);

                connect(KeywordModel, &cwKeywordEntityFilterModel::rowsAboutToBeRemoved,
                        this, [this, pairs](const QModelIndex& parent, int begin, int end)
                {
                    Q_UNUSED(parent);
                    Q_UNUSED(begin);
                    Q_UNUSED(end);

                    auto filter = createDefaultFilter();
                    for(auto pair : pairs) {
                        filter.filterEntity(pair);
                    }

                    //Disconnect the current slot
                    disconnect(KeywordModel, &cwKeywordEntityFilterModel::rowsAboutToBeRemoved,
                               this, nullptr);
                });
            });
        }

        updateAllRows();

        emit keywordModelChanged();
    }
}

int cwKeywordEntityFilterModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Data.size();
}

QVariant cwKeywordEntityFilterModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    auto row = Data.row(index.row());

    switch(role) {
    case EntitiesRole:
        return QVariant::fromValue(row.entities);
    case EntitiesCountRole:
        return row.entities.size();
    case ValueRole:
        return row.value;
    }

    return QVariant();
}

QString cwKeywordEntityFilterModel::otherCategory()
{
    return QStringLiteral("Other");
}

void cwKeywordEntityFilterModel::waitForFinished()
{
    cwAsyncFuture::waitForFinished(LastRun);
    Q_ASSERT(isRunning() == false);
}

void cwKeywordEntityFilterModel::updateAllRows()
{
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

        for(auto entity : entities) {
            insertRow.filterEntity(entity);
        }

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
        Data = runFuture.result();
        endResetModel();
    }
    ).future();
}

bool cwKeywordEntityFilterModel::isRunning() const
{
    return LastModelData.isRunning();
}

cwKeywordEntityFilterModel::Filter cwKeywordEntityFilterModel::createDefaultFilter()
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
void cwKeywordEntityFilterModel::setKeywords(QList<cwKeyword> keywords) {
    if(Keywords != keywords) {
        Keywords = keywords;
        updateAllRows();
        emit keywordsChanged();
    }
}

/**
*
*/
void cwKeywordEntityFilterModel::setLastKey(QString keyLast) {
    if(LastKey != keyLast) {
        LastKey = keyLast;
        updateAllRows();
        emit keyLastChanged();
    }
}

void cwKeywordEntityFilterModel::Filter::addEntity(const QString &value,
                                                      QEntity *entity)
{
    auto& rows = data->Rows;

    auto iter = std::lower_bound(rows.begin(), rows.end(), value, &lessThan);

    if(iter != rows.end() && iter->value == value) {
        //Add to an existing row, dataChanged
        iter->entities.append(entity);
        dataChanged(iter);
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

void cwKeywordEntityFilterModel::Filter::removeEntity(const QString &value, QEntity *entity)
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

            data->OtherRow.entities.append(entity);
            dataChanged(data->otherRowIndex());
        }
    }
}

void cwKeywordEntityFilterModel::Filter::beginInsert(const QVector<Row>::iterator &iter) const
{
    if(beginInsertFunction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginInsertFunction(index);
    }
}

void cwKeywordEntityFilterModel::Filter::endInsert() const
{
    if(endInsertFunction) {
        endInsertFunction();
    }
}

void cwKeywordEntityFilterModel::Filter::beginRemove(const QVector<Row>::iterator &iter) const
{
    if(beginRemoveFuction) {
        int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
        beginRemoveFuction(index);
    }
}

void cwKeywordEntityFilterModel::Filter::endRemove() const
{
    if(endInsertFunction) {
        endInsertFunction();
    }
}

void cwKeywordEntityFilterModel::Filter::dataChanged(const QVector<Row>::iterator &iter) const
{
    int index = static_cast<int>(std::distance(data->Rows.begin(), iter));
    dataChanged(index);
}

void cwKeywordEntityFilterModel::Filter::dataChanged(int index) const
{
    if(dataChangedFunction) {
        dataChangedFunction(index, {EntitiesRole, EntitiesCountRole});
    }
}

bool cwKeywordEntityFilterModel::Filter::lessThan(const cwKeywordEntityFilterModel::Row &row, const QString &value)
{
    return row.value < value;
}

void cwKeywordEntityFilterModel::Filter::filterEntity(const cwKeywordEntityFilterModel::EntityAndKeywords &pair)
{
    bool hasAllKeywords = true;
    for(auto keyword : keywords) {
        hasAllKeywords = hasAllKeywords && pair.keywords.contains(keyword);
        if(!hasAllKeywords) {
            //Missing one of the keywords
            removeEntity(keyword.value(), pair.entity);
            break;
        }
    }

    if(hasAllKeywords) {
        hasAllKeywords = false;
        for(auto keyword : pair.keywords) {
            if(keyword.key() == lastKey) {
                addEntity(keyword.value(), pair.entity);
                hasAllKeywords = true;
                break;
            }
        }
    }

    if(!hasAllKeywords) {
        //Add the entity to the Other row
        removeEntity(lastKey, pair.entity);
        data->OtherRow.entities.append(pair.entity);
    }
}

cwKeywordEntityFilterModel::EntityAndKeywords::EntityAndKeywords(const QModelIndex &entityIndex)
{
    Q_ASSERT(entityIndex.parent() == QModelIndex());
    entity = entityIndex.data(cwKeywordEntityModel::EntityRole).value<QEntity*>();
    keywords = entityIndex.data(cwKeywordEntityModel::KeywordsRole).value<QVector<cwKeyword>>();
}
