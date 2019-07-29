//Our includes
#include "cwKeywordEntityFilterModel.h"
#include "cwKeyword.h"

//Qt includes
using namespace Qt3DCore;

cwKeywordEntityFilterModel::cwKeywordEntityFilterModel(QObject *parent) :
    QAbstractItemModel(parent),
    RootList(std::make_unique<List>())
{

}

/**
*
*/
void cwKeywordEntityFilterModel::setKeywordModel(cwKeywordEntityModel* keywordModel) {
    if(KeywordModel != keywordModel) {
        beginResetModel();
        KeywordModel = keywordModel;

        RootList->clearChildren();
        addEntities(RootList.get());

        endResetModel();

        emit keywordModelChanged();
    }
}

/**
 * @brief cwKeywordEntityFilterModel::addKey
 * @param key
 */
void cwKeywordEntityFilterModel::addKey(const QString &key)
{
    Keys.append(key);
    int lastKeyIndex = Keys.size() - 1;

    std::function<void (List*)> addNewKey;
    addNewKey = [&addNewKey, this, lastKeyIndex](List* rootList) {
        if(rootList->keyIndex == lastKeyIndex) {
            rootList->clearChildren();
            addEntities(rootList);
        } else {
            for(auto child : rootList->children) {
                addNewKey(child);
            }
        }
    };

    addNewKey(RootList.get());

    emit keysChanged();
}

int cwKeywordEntityFilterModel::rowCount(const QModelIndex &parent) const
{
    auto list = toList(parent);
    return list->children.size();
}

int cwKeywordEntityFilterModel::columnCount(const QModelIndex &parent) const
{
    return 0;
}

QModelIndex cwKeywordEntityFilterModel::parent(const QModelIndex &index) const
{
    List* parentList = static_cast<List*>(index.internalPointer());
    List* parentParentList = parentList->parent;
    int row = parentParentList->children.indexOf(parentList);
    return createIndex(row, 0, parentParentList);
}

QModelIndex cwKeywordEntityFilterModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(column);

    List* list = toList(parent);
    if(row >= 0 && row < list->children.size()) {
        return createIndex(row, 0, list);
    }
    return QModelIndex();
}

QVariant cwKeywordEntityFilterModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    List* list = toList(index);

    switch(role) {
    case EntitiesRole:
        return QVariant::fromValue(list->entities);
    case EntitiesCountRole:
        return list->entities.size();
    case ValueRole:
        return list->value;
    }

    return QVariant();
}

void cwKeywordEntityFilterModel::addEntity(const QModelIndex &entityIndex,
                                           List* rootList)
{
    auto entity = entityIndex.data(cwKeywordEntityModel::EntityRole).value<QEntity*>();

    const QString& key = rootList->keyIndex < Keys.size() ? Keys.at(rootList->keyIndex) : QString();

    auto model = entityIndex.model();
    int rowCount = model->rowCount(entityIndex);
    bool hasBeenAdded = false;
    for(int i = 0; i < rowCount; i++) {
        //For each keyword for the entity
        auto keywordIndex = model->index(i, 0, entityIndex);
        cwKeyword keyword = keywordIndex.data(cwKeywordEntityModel::KeywordRole).value<cwKeyword>();

        if(keyword.key() == key) {
            //Entity has key
            rootList->addChild(keyword.value(), entity);
            hasBeenAdded = true;
        }
    }

    if(hasBeenAdded) {
       entity = nullptr;
    }

    rootList->addChild("Everything Else", entity);
}

void cwKeywordEntityFilterModel::addEntities(cwKeywordEntityFilterModel::List *rootList)
{
    if(KeywordModel) {
        for(int i = 0; i < KeywordModel->rowCount(); i++) {
            auto index = KeywordModel->index(i, 0, QModelIndex());
            addEntity(index, rootList);
        }
    }
}

cwKeywordEntityFilterModel::List *cwKeywordEntityFilterModel::toList(const QModelIndex &index) const
{
    List* list;
    if(index == QModelIndex()) {
        list = RootList.get();
    } else {
        auto parentList = static_cast<List*>(index.internalPointer());
        list = parentList->children.at(index.row());
    }
    return list;
}

cwKeywordEntityFilterModel::List::~List()
{
    deleteChildren();
}

void cwKeywordEntityFilterModel::List::clearChildren()
{
    deleteChildren();
    children.clear();
}

void cwKeywordEntityFilterModel::List::deleteChildren()
{
    for(auto child : children) {
        delete child;
    }
}

void cwKeywordEntityFilterModel::List::addChild(const QString &value, QEntity *entity)
{
    List* child;
    auto iter = std::lower_bound(children.begin(), children.end(), value, &List::lessThan);
    if(iter != children.end() && (*iter)->value == value) {
        //Found the child
        child = *iter;
    } else {
        //Add new child
        child = new List(value);
        children.insert(iter, child);
        child->parent = this;
        child->keyIndex = keyIndex + 1;
    }

    if(entity != nullptr) {
        if(!child->entities.contains(entity)) {
            child->entities.append(entity);
        }
    }

}
