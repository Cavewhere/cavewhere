//Our includes
#include "cwEntityKeywordsModel.h"

//Std includes
#include <algorithm>

cwEntityKeywordsModel::cwEntityKeywordsModel(QObject *parent) : QAbstractListModel(parent)
{

}

void cwEntityKeywordsModel::insert(const cwEntityAndKeywords &row)
{
    auto iter = std::lower_bound(mRows.begin(), mRows.end(), row, entityKeywordsLessThan);
    int index = std::distance(mRows.begin(), iter);
    if(iter->entity() == row.entity()) {
        //Update the row
        *iter = row;
        auto modelIndex = this->index(index);
        emit dataChanged(modelIndex, modelIndex, {KeywordsRole,EntityKeywordsRole});
    } else {
        beginInsertRows(QModelIndex(), index, index);
        mRows.insert(iter, row);
        endInsertRows();
    }
}

void cwEntityKeywordsModel::remove(QObject *entity)
{
    auto iter = std::lower_bound(mRows.begin(), mRows.end(), entity, entityLessThan);
    if(iter != mRows.end()) {
        int index = std::distance(mRows.begin(), iter);
        beginRemoveRows(QModelIndex(), index, index);
        mRows.erase(iter);
        endRemoveRows();
    }
}

int cwEntityKeywordsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return mRows.size();
}

QVariant cwEntityKeywordsModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    const auto& row = mRows.at(index.row());

    switch(role) {
    case EntityKeywordsRole:
        return QVariant::fromValue(row);
    case EntityRole:
        return QVariant::fromValue(row.entity());
    case KeywordsRole:
        return QVariant::fromValue(row.keywords());
    }

    return QVariant();
}

bool cwEntityKeywordsModel::entityKeywordsLessThan(const cwEntityAndKeywords &a, const cwEntityAndKeywords &b)
{
    return a.entity() < b.entity();
}

bool cwEntityKeywordsModel::entityLessThan(const cwEntityAndKeywords &a, QObject *b)
{
    return a.entity() < b;
}
