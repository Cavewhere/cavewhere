#include "cwKeywordModel.h"

cwKeywordModel::cwKeywordModel(QObject *parent) :
    QAbstractListModel (parent)
{

}

/**
 Adds a keyword and the value to the model
 */
void cwKeywordModel::add(const cwKeyword &keyword)
{
    if(!Keywords.contains(keyword) && keyword.isValid()) {
        auto last = static_cast<int>(std::distance(Keywords.begin(), Keywords.end()));
        beginInsertRows(QModelIndex(), last, last);
        Keywords.append(keyword);
        endInsertRows();
    }
}


void cwKeywordModel::remove(const cwKeyword &keyword)
{
    if(keyword.isValid()) {
        int index = Keywords.indexOf(keyword);
        if(index >= 0) {
            beginRemoveRows(QModelIndex(), index, index);
            Keywords.removeAt(index);
            endRemoveRows();
        }
    }
}

void cwKeywordModel::removeAll(QString key)
{
    for(auto iter = Keywords.begin(); iter != Keywords.end();) {
        if(iter->key() == key) {
            int index = std::distance(Keywords.begin(), iter);
            beginRemoveRows(QModelIndex(), index, index);
            iter = Keywords.erase(iter);
            endRemoveRows();
        } else {
            iter++;
        }
    }
}

/**

 */
QVariant cwKeywordModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }
    const auto& keyword = Keywords.at(index.row());
    switch (role) {
    case KeyRole:
        return keyword.key();
    case ValueRole:
        return keyword.value();
    case KeywordRole:
        return QVariant::fromValue(keyword);
    }
    return QVariant();
}

/**
 * Sets the data for the keyword model.
 *
 * This will return false if the modified Keyword already exists in the model. The role must
 * be Key or Value from the Role enum (if not, nothing happens and returns false). Index must be valid.
 */
bool cwKeywordModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid()) { return false; }
    auto newKeyword = Keywords.at(index.row());
    QVector<int> roles;
    switch (role) {
    case KeyRole:
        newKeyword.setKey(value.toString());
        roles = {KeyRole, KeywordRole};
        break;
    case ValueRole:
        newKeyword.setValue(value.toString());
        roles = {ValueRole, KeywordRole};
        break;
    case KeywordRole:
        roles = {KeywordRole};
        newKeyword = value.value<cwKeyword>();
        break;
    default:
        return false;
    }

    if(!Keywords.contains(newKeyword)) {
        Keywords[index.row()] = newKeyword;
        emit dataChanged(index, index, {roles});
        return true;
    }

    return false;
}

/**
 * Returns the role names
 */
QHash<int, QByteArray> cwKeywordModel::roleNames() const
{
    return {
        {KeyRole, "keyRole"},
        {ValueRole, "valueRole"},
        {KeywordRole, "keywordRole"}
    };
}
