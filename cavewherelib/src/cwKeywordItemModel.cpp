//Our includes
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwKeywordItem.h"

//Qt includes
#include <QPointer>
#include <QDebug>

cwKeywordItemModel::cwKeywordItemModel(QObject *parent) :
    QAbstractItemModel (parent)
{

}

void cwKeywordItemModel::addItem(cwKeywordItem *item)
{
    //    auto add = [item, this]() {
    auto iter = std::lower_bound(Items.begin(), Items.end(), item);
    int index = static_cast<int>(std::distance(Items.begin(), iter));

    item->setParent(this);

    //Make sure entity doesn't already exist, entity has two cwKeywordComponent, which isn't allowed
//    Q_ASSERT((*iter)->object() != item->object());

    beginInsertRows(QModelIndex(), index, index);
    Items.insert(iter, item);
    endInsertRows();

    connect(item->keywordModel(), &cwKeywordModel::rowsAboutToBeInserted,
            this, [item, this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent)
        auto index = indexOf(item);
        beginInsertRows(index, first, last);
    });

    connect(item->keywordModel(), &cwKeywordModel::rowsInserted,
            this, [this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent)
        Q_UNUSED(first)
        Q_UNUSED(last)
        endInsertRows();
    });

    connect(item->keywordModel(), &cwKeywordModel::rowsAboutToBeRemoved,
            this, [item, this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent)
        auto index = indexOf(item);
        beginRemoveRows(index, first, last);
    });

    connect(item->keywordModel(), &cwKeywordModel::rowsRemoved,
            this, [this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent)
        Q_UNUSED(first)
        Q_UNUSED(last)
        endRemoveRows();
    });

    connect(item->keywordModel(), &cwKeywordModel::dataChanged,
            this, [this, item](const QModelIndex& topLeft, const QModelIndex& bottomRight, QVector<int> roles)
    {
        static const QHash<int, int> roleToRole = {
            {cwKeywordModel::KeyRole, KeyRole},
            {cwKeywordModel::ValueRole, ValueRole}
        };

        for(int& role : roles) {
            role = roleToRole.value(role, -1);
        }

        //Remove all unsupported roles
        roles.removeAll(-1);

        if(roles.isEmpty()) { return; }

        auto newTopLeft = createIndex(topLeft.row(), 0, item);
        auto newBottomRight = createIndex(bottomRight.row(), 0, item);

        emit dataChanged(newTopLeft, newBottomRight, roles);
    });

    connect(item, &cwKeywordItem::objectChanged, this, [item, this]() {
        auto modelIndex = indexOf(item);
        emit dataChanged(modelIndex, modelIndex, {ObjectRole});
    });

    connect(item, &cwKeywordItem::destroyed, this, [item, this]() {
        remove(item);
    });
}

void cwKeywordItemModel::removeItem(cwKeywordItem *item)
{
    remove(item);
    disconnect(item, nullptr, this, nullptr);
}

cwKeywordItem *cwKeywordItemModel::item(int row) const
{
    return Items.at(row);
}

QVariant cwKeywordItemModel:: data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    if(index.internalPointer() == nullptr) {
        switch(role) {
        case ItemRole:
            return QVariant::fromValue(Items.at(index.row()));
        case ObjectRole:
        {
            auto object = Items.at(index.row())->object();
            return QVariant::fromValue(object);
        }
        case KeywordModelRole:
        {
            auto model = keywordModel(index);
            return QVariant::fromValue(model);
        }
        case KeywordsRole:
            auto model = keywordModel(index);
            return QVariant::fromValue(model->keywords());
        }
    }

    if(!index.parent().isValid()) {
        return QVariant();
    }

    auto model = keywordModel(index.parent());
    auto modelIndex = model->index(index.row());
    switch (role) {
    case KeyRole:
        return modelIndex.data(cwKeywordModel::KeyRole);
    case ValueRole:
        return modelIndex.data(cwKeywordModel::ValueRole);
    case KeywordRole:
        return modelIndex.data(cwKeywordModel::KeywordRole);
    }

    return QVariant();
}

int cwKeywordItemModel::rowCount(const QModelIndex &parent) const
{
    if(parent == QModelIndex()) {
        return Items.size();
    }

    auto childModel = keywordModel(parent);
    if(childModel) {
        return childModel->rowCount();
    }

    return 0;
}

int cwKeywordItemModel::columnCount(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return 0;
}

QModelIndex cwKeywordItemModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(column)

    if(row < 0) {
        return QModelIndex();
    }

    if(parent == QModelIndex()) {
        if(row >= Items.size()) { return QModelIndex(); }
        return createIndex(row, 0);

    } else if(parent.internalPointer() == nullptr) {
        Q_ASSERT(parent.model() == this);
        auto item = Items.at(parent.row());
        if(row >= item->keywordModel()->rowCount()) {
            return QModelIndex();
        }

        return createIndex(row, 0, item);
    }

    return QModelIndex();
}

QModelIndex cwKeywordItemModel::parent(const QModelIndex &index) const
{
    if(!index.internalPointer()) {
        return QModelIndex();
    }
    return createIndex(indexOf(index), 0, nullptr);
}

QModelIndex cwKeywordItemModel::indexOf(cwKeywordItem* item) const
{
    auto iter = std::lower_bound(Items.begin(), Items.end(), item);
    int index = static_cast<int>(std::distance(Items.begin(), iter));

    if(index < 0 || index >= Items.size()) {
        return QModelIndex();
    }

    if(Items.at(index) == item) {
        return createIndex(index, 0, nullptr);
    }

    return QModelIndex();
}

//QVector<cwEntityAndKeywords> cwKeywordItemModel::entityAndKeywords() const
//{
//    QVector<cwEntityAndKeywords> entities;
//    entities.reserve(Items.size());

//    for(auto item : Items) {
//        entities.append(cwEntityAndKeywords(item->object(),
//                                            item->keywordModel()->keywords()));
//    }

//    return entities;
//}

cwKeywordModel *cwKeywordItemModel::keywordModel(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    auto item = Items.at(index.row());
    return item->keywordModel(); //Return the keyword model for the QEntity
}

void cwKeywordItemModel::remove(cwKeywordItem* item, DisconnectionType type)
{
    Q_ASSERT(item);

    auto iter = std::lower_bound(Items.begin(), Items.end(), item);

    if(iter == Items.end()) {
        //Item doesn't exist
        return;
    }

    Q_ASSERT(*iter == item);

    int index = static_cast<int>(std::distance(Items.begin(), iter));
    beginRemoveRows(QModelIndex(), index, index);

    if(type == DisconnectAll) {
        //This assumes that item still exists in memory and hasn't been deleted
        auto item = *iter;
        disconnect(item->keywordModel(), nullptr, this, nullptr);
    }

    Items.erase(iter);
    endRemoveRows();
}

int cwKeywordItemModel::indexOf(const QModelIndex &modelIndex) const
{
    auto item = static_cast<cwKeywordItem*>(modelIndex.internalPointer());
    auto iter = std::lower_bound(Items.begin(), Items.end(), item);
    return static_cast<int>(std::distance(Items.begin(), iter));
}
