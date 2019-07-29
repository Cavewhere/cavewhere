//Our includes
#include "cwKeywordEntityModel.h"
#include "cwKeywordComponent.h"
#include "cwKeywordModel.h"

//Qt includes
#include <QPointer>

using namespace Qt3DCore;

cwKeywordEntityModel::cwKeywordEntityModel(QObject *parent) :
    QAbstractItemModel (parent)
{

}

void cwKeywordEntityModel::addComponent(cwKeywordComponent *component)
{
    auto add = [component, this](Qt3DCore::QEntity* entity) {
        Q_ASSERT(entity);

        KeywordEntity newKeywordEntity(component, entity);
        auto iter = std::lower_bound(Entities.begin(), Entities.end(), newKeywordEntity);
        int index = static_cast<int>(std::distance(Entities.begin(), iter));

        //Make sure entity doesn't already exist, entity has two cwKeywordComponent, which isn't allowed
        Q_ASSERT(iter->entity != entity);

        beginInsertRows(QModelIndex(), index, index);
        Entities.insert(iter, newKeywordEntity);
        endInsertRows();

        auto connectKeywordModel = [entity, this](cwKeywordModel* keywordModel){
            if(keywordModel == nullptr) { return;};

            connect(keywordModel, &cwKeywordModel::rowsAboutToBeInserted,
                    entity, [keywordModel, entity, this](const QModelIndex& parent, int first, int last)
            {
                Q_UNUSED(parent);
                auto index = indexOf(entity);
                beginInsertRows(index, first, last);
            });

            connect(keywordModel, &cwKeywordModel::rowsInserted,
                    entity, [this](const QModelIndex& parent, int first, int last)
            {
                Q_UNUSED(parent);
                Q_UNUSED(first);
                Q_UNUSED(last);
                endInsertRows();
            });

            connect(keywordModel, &cwKeywordModel::rowsAboutToBeRemoved,
                    entity, [entity, this](const QModelIndex& parent, int first, int last)
            {
                Q_UNUSED(parent);
                auto index = indexOf(entity);
                beginRemoveRows(index, first, last);
            });

            connect(keywordModel, &cwKeywordModel::rowsRemoved,
                    entity, [this](const QModelIndex& parent, int first, int last)
            {
                Q_UNUSED(parent);
                Q_UNUSED(first);
                Q_UNUSED(last);
                endRemoveRows();
            });

            connect(keywordModel, &cwKeywordModel::dataChanged,
                    entity, [this, entity](const QModelIndex& topLeft, const QModelIndex& bottomRight, QVector<int> roles)
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

                auto newTopLeft = createIndex(topLeft.row(), 0, entity);
                auto newBottomRight = createIndex(bottomRight.row(), 0, entity);

                emit dataChanged(newTopLeft, newBottomRight, roles);
            });
        };

        connectKeywordModel(component->keywordModel());
    };

    for(auto entity : component->entities()) {
        add(entity);
    }

    connect(component, &cwKeywordComponent::addedToEntity, this, add);
    connect(component, &cwKeywordComponent::removedFromEntity, this, &cwKeywordEntityModel::remove);
}

void cwKeywordEntityModel::removeComponent(cwKeywordComponent *component)
{
    for(auto entity : component->entities()) {
        remove(entity);
    }

    disconnect(component, nullptr, this, nullptr);
}

QVariant cwKeywordEntityModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    if(index.internalPointer() == nullptr && role == EntityRole) {
        auto entity = Entities.at(index.row()).entity;
        return QVariant::fromValue(entity);
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

int cwKeywordEntityModel::rowCount(const QModelIndex &parent) const
{
    if(parent == QModelIndex()) {
        return Entities.size();
    }

    auto childModel = keywordModel(parent);
    if(childModel) {
        return childModel->rowCount();
    }

    return 0;
}

int cwKeywordEntityModel::columnCount(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return 0;
}

QModelIndex cwKeywordEntityModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(column);

    if(row < 0) {
        return QModelIndex();
    }

    if(parent == QModelIndex()) {
        if(row >= Entities.size()) { return QModelIndex(); }
        return createIndex(row, 0);

    } else if(parent.internalPointer() == nullptr) {
        Q_ASSERT(parent.model() == this);
        auto keywordEntity = Entities.at(parent.row());
        if(row >= keywordEntity.keywordComponent->keywordModel()->rowCount()) {
            return QModelIndex();
        }

        return createIndex(row, 0, keywordEntity.entity);
    }

    return QModelIndex();
}

QModelIndex cwKeywordEntityModel::parent(const QModelIndex &index) const
{
    if(!index.internalPointer()) {
        return QModelIndex();
    }
    return createIndex(indexOf(index), 0, nullptr);
}

QModelIndex cwKeywordEntityModel::indexOf(QEntity *entity) const
{
    auto iter = std::lower_bound(Entities.begin(), Entities.end(), KeywordEntity(nullptr, entity));
    int index = static_cast<int>(std::distance(Entities.begin(), iter));

    if(index < 0 || index >= Entities.size()) {
        return QModelIndex();
    }

    if(Entities.at(index).entity == entity) {
        return createIndex(index, 0, nullptr);
    }

    return QModelIndex();
}

cwKeywordModel *cwKeywordEntityModel::keywordModel(const QModelIndex &index) const
{
    auto realKeyword = Entities.at(index.row());
    return realKeyword.keywordComponent->keywordModel(); //Return the keyword model for the QEntity
}

void cwKeywordEntityModel::remove(Qt3DCore::QEntity *entity)
{
    Q_ASSERT(entity);

    KeywordEntity removeKeywordEntity(nullptr, entity);
    auto iter = std::lower_bound(Entities.begin(), Entities.end(), removeKeywordEntity);
    Q_ASSERT(iter->entity == entity);

    int index = static_cast<int>(std::distance(Entities.begin(), iter));
    beginRemoveRows(QModelIndex(), index, index);

    auto component = iter->keywordComponent;
    disconnect(component->keywordModel(), nullptr, entity, nullptr);

    Entities.erase(iter);
    endRemoveRows();

}

int cwKeywordEntityModel::indexOf(const QModelIndex &modelIndex) const
{
    QEntity* entity = static_cast<QEntity*>(modelIndex.internalPointer());
    KeywordEntity keyword(nullptr, entity);

    auto iter = std::lower_bound(Entities.begin(), Entities.end(), keyword);
    return static_cast<int>(std::distance(Entities.begin(), iter));
}
