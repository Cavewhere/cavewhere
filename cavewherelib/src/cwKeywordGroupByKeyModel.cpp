//Our includes
#include "cwKeywordGroupByKeyModel.h"
#include "cwKeyword.h"
#include "cwGlobals.h"
#include "cwKeywordFilterModel.h"

//Qt includes
#include <QtConcurrent>
#include <QQuickItem>

cwKeywordGroupByKeyModel::cwKeywordGroupByKeyModel(QObject *parent) :
    QAbstractListModel(parent),
    mAcceptedModel(new cwKeywordFilterModel(this))
{
    mFilter = createDefaultFilter();
}

cwKeywordGroupByKeyModel::~cwKeywordGroupByKeyModel()
{
    // qDebug() << "Deleting:" << this << "also deleting:" << mAcceptedModel;
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
        for(const auto& index : std::as_const(row.indexes)) {
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
        {AcceptedRole, "acceptedRole"}
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
    }
    //TODO: update the visiblity of the rendering elements
    /*else if(auto entity = dynamic_cast<Qt3DCore::QEntity*>(object)) {
        item->setEnabled(accepted);
    }*/
}

void cwKeywordGroupByKeyModel::updateAllRows()
{
    beginResetModel();
    mData.clear();

    if(mSourceModel) {
        //Use an empty filter to prevent signals from being emitting
        Filter filter;
        filter.model = this;
        filter.key = mKey;

        for(int i = 0; i < mSourceModel->rowCount(); i++) {
            auto index = mSourceModel->index(i, 0, QModelIndex());
            filter.filterEntity(index);
        }      
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
    insertRow.model = this;
    insertRow.beginRemoveFuction = beginRemoveFunction;
    insertRow.endRemoveFunction = endRemoveFunction;
    insertRow.beginInsertFunction = beginInsertFunction;
    insertRow.endInsertFunction = endInsertFunction;
    insertRow.dataChangedFunction = dataChangeFunction;

    return insertRow;
}

void cwKeywordGroupByKeyModel::setAcceptIndex(const QModelIndex &index, bool accepted) {
    if(accepted) {
        mAcceptedModel->insert(index);
    } else {
        mAcceptedModel->remove(index);
    }
}

/**
*
*/
void cwKeywordGroupByKeyModel::setKey(QString keyLast) {
    if(mKey != keyLast) {
        mKey = keyLast;
        mFilter.key = mKey;
        updateAllRows();

        //Accept or reject other category if mkey isn't set
        setData(otherIndex(), mKey.isEmpty(), AcceptedRole);

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
    auto& rows = model->mData.Rows;
    auto iter = std::lower_bound(rows.begin(), rows.end(), value, &lessThan);

    if(iter != rows.end() && iter->value == value) {
        if(!iter->indexes.contains(sourceIndex)) {
            //Add to an existing row, dataChanged
            iter->indexes.append(sourceIndex);
            model->setAcceptIndex(sourceIndex, iter->accepted);
            dataChanged(iter);
        }
    } else {
        beginInsert(iter);
        rows.insert(iter, Row(value, {sourceIndex}, model->acceptByDefault()));
        model->setAcceptIndex(sourceIndex, model->acceptByDefault());
        endInsert();
    }

    bool removed = model->mData.OtherRow.indexes.removeOne(sourceIndex);
    if(removed) {
        dataChanged(model->mData.otherRowIndex());
    }
}

void cwKeywordGroupByKeyModel::Filter::addEntityToOther(const QModelIndex& sourceIndex)
{
    if(!model->mData.OtherRow.indexes.contains(sourceIndex)) {
        model->mData.OtherRow.indexes.append(sourceIndex);
        model->setAcceptIndex(sourceIndex, model->mData.OtherRow.accepted);
        dataChanged(model->mData.otherRowIndex());
    }
}

void cwKeywordGroupByKeyModel::Filter::removeEntity(const QString &value, const QModelIndex& sourceIndex)
{
    auto& rows = model->mData.Rows;
    auto iter = std::lower_bound(rows.begin(), rows.end(), value, &lessThan);

    if(iter != rows.end() && iter->value == value) {
        bool removed = removeEntityFromRow(iter, sourceIndex);
        if(removed) {
            bool stillContainsEntity = false;
            for(const auto& row : rows) {
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
    return removeEntityFromRow(std::distance(model->mData.Rows.begin(), iter), *iter, sourceIndex);
}

bool cwKeywordGroupByKeyModel::Filter::removeEntityFromRow(int i, Row &row, const QModelIndex& sourceIndex)
{
    bool removed = row.indexes.removeOne(sourceIndex);
    if(removed) {
        if(row.indexes.isEmpty() && &row != &model->mData.OtherRow) {
            //No more entities
            beginRemoveFuction(i);
            model->mData.Rows.removeAt(i);
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
        int index = static_cast<int>(std::distance(model->mData.Rows.begin(), iter));
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
        int index = static_cast<int>(std::distance(model->mData.Rows.begin(), iter));
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
    int index = static_cast<int>(std::distance(model->mData.Rows.begin(), iter));
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

    for(const auto& keyword : std::as_const(keywords)) {
        //One of sourceIndex's keys matches the key were's filtering by
        if(keyword.key() == key) {
            if(!oldValues.contains(keyword.value())) {
                addEntity(keyword.value(), sourceIndex);
            } else {
                //Doesn't have to be removed
                oldValues.remove(keyword.value());
            }
        }
    }

    for(const auto& valueToRemove : std::as_const(oldValues)) {
        removeEntity(valueToRemove, sourceIndex);
    }
}

void cwKeywordGroupByKeyModel::Filter::removeEntityFromAllRows(const QModelIndex& sourceIndex)
{
    for(auto iter = model->mData.Rows.begin(); iter < model->mData.Rows.end(); iter++) {
        removeEntityFromRow(iter, sourceIndex);
    }
    removeEntityFromRow(model->mData.otherRowIndex(), model->mData.OtherRow, sourceIndex);
}

QSet<QString> cwKeywordGroupByKeyModel::Filter::entityValues(const QModelIndex& sourceIndex) const
{
    QSet<QString> valuesOfEntity;
    for(const auto& row : std::as_const(model->mData.Rows)) {
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

        mAcceptedModel->setSourceModel(mSourceModel);

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
                    if(topLeft.parent().isValid()) {
                        updateEntity(topLeft.parent().row());
                    } else {
                        for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
                            updateEntity(i);
                        }
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
                } else {
                    updateEntity(parent.row());
                }
            });

            connect(mSourceModel, &QAbstractItemModel::rowsAboutToBeRemoved,
                    this, [this, toIndex, updateEntity](const QModelIndex& parent, int begin, int last)
            {
                if(parent == QModelIndex()) {
                    for(int i = begin; i <= last; i++) {
                        mFilter.removeEntityFromAllRows(toIndex(i));
                    }
                } else {
                    updateEntity(parent.row());
                }
            });

            connect(mSourceModel, &QAbstractItemModel::rowsRemoved,
                    this, [updateEntity](const QModelIndex& parent, int begin, int last)
            {
                Q_UNUSED(begin)
                Q_UNUSED(last)
                if(parent != QModelIndex()) {
                    updateEntity(parent.row());
                }
            });

            updateAllRows();
        }

        emit sourceChanged();
    }
}

void cwKeywordGroupByKeyModel::setAcceptedByDefault(bool acceptByDefault) {
    if(mAcceptedByDefault != acceptByDefault) {
        mAcceptedByDefault = acceptByDefault;
        emit acceptByDefaultChanged();
    }
}
