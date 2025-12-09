//Our includes
#include "cwKeywordModel.h"

//Qt includes
#include <QDebug>

const QString cwKeywordModel::TypeKey = QStringLiteral("Type");
const QString cwKeywordModel::TripNameKey = QStringLiteral("Trip");
const QString cwKeywordModel::YearKey = QStringLiteral("Year");
const QString cwKeywordModel::DateKey = QStringLiteral("Date");
const QString cwKeywordModel::CaverKey = QStringLiteral("Caver");
const QString cwKeywordModel::FileNameKey = QStringLiteral("File Name");
const QString cwKeywordModel::ObjectIdKey = QStringLiteral("Object id");

cwKeywordModel::cwKeywordModel(QObject *parent) :
    QAbstractListModel (parent)
{

}

/**
 Adds a keyword and the value to the model
 */
void cwKeywordModel::add(const cwKeyword &keyword)
{
    if(canKeywordBeAdded(keyword)) {
        auto last = static_cast<int>(std::distance(Keywords.begin(), Keywords.end()));
        beginInsertRows(QModelIndex(), last, last);
        Keywords.append(keyword);
        CachedRowCount++;
        endInsertRows();
    }
}

/**
 Adds multiple keywords to the model
 */
void cwKeywordModel::addKeywords(const QVector<cwKeyword> &keywords)
{
    QVector<cwKeyword> validKeywords;
    validKeywords.reserve(keywords.size());
    for(auto keyword : keywords) {
        if(canKeywordBeAdded(keyword)) {
            validKeywords.append(keyword);
        }
    }

    auto first = static_cast<int>(std::distance(Keywords.begin(), Keywords.end()));
    auto last = first + validKeywords.size() - 1;
    beginInsertRows(QModelIndex(), first, last);
    Keywords.append(validKeywords);
    CachedRowCount += validKeywords.size();
    endInsertRows();
}


void cwKeywordModel::remove(const cwKeyword &keyword)
{
    if(keyword.isValid()) {
        int index = Keywords.indexOf(keyword);
        if(index >= 0) {
            beginRemoveRows(QModelIndex(), index, index);
            Keywords.removeAt(index);
            CachedRowCount--;
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
            CachedRowCount--;
            endRemoveRows();
        } else {
            iter++;
        }
    }
}

void cwKeywordModel::replace(const cwKeyword &keyword)
{
    removeAll(keyword.key());
    add(keyword);
}

void cwKeywordModel::remove(int index)
{
    if(index >= 0 && index < Keywords.size()) {
        beginRemoveRows(QModelIndex(), index, index);
        CachedRowCount--;
        Keywords.removeAt(index);
        endRemoveRows();
    }
}

void cwKeywordModel::clear()
{
    if(!Keywords.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, Keywords.size() - 1);
        CachedRowCount -= Keywords.size();
        Keywords.clear();
        endRemoveRows();
    }
}

void cwKeywordModel::setKeywords(const QVector<cwKeyword> &keywords)
{
    clear();
    addKeywords(keywords);
}

/**
Returns the data from the model
 */
QVariant cwKeywordModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    if(index.row() < Keywords.size()) {
        const auto& keyword = Keywords.at(index.row());
        switch (role) {
        case KeyRole:
            return keyword.key();
        case ValueRole:
            return keyword.value();
        case KeywordRole:
            return QVariant::fromValue(keyword);
        }
    } else {
        auto extension = modelAt(index);
        auto model = extension.first;
        auto modelRow = extension.second;
        Q_ASSERT(model); //If this fails index generation is broken, and is a bug
        if(model == nullptr) {
            return QVariant();
        }
        return model->data(model->index(modelRow), role);
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

    if(index.row() < Keywords.size()) {
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
    } else {
        auto extension = modelAt(index);
        auto model = extension.first;
        auto modelRow = extension.second;
        return model->setData(model->index(modelRow), value, role);
    }

    return false;
}

/**
 * Extend this keyword model with model
 */
void cwKeywordModel::addExtension(cwKeywordModel *model)
{
    if(model == nullptr) {
        return;
    }

    if(Extentions.contains(model)) {
        return; //Model already exists
    }

    connect(model, &cwKeywordModel::destroyed,
            this, [model, this]()
    {
        removeExtension(model);
    });

    connect(model, &cwKeywordModel::dataChanged,
            this, [model, this](const QModelIndex& topLeft, const QModelIndex& bottomRight, QVector<int> roles)
    {
        auto topLeftLocal = localIndex(model, topLeft.row());
        auto bottomRightLocal = localIndex(model, bottomRight.row());
        dataChanged(index(topLeftLocal), index(bottomRightLocal), roles);
    });

    connect(model, &cwKeywordModel::rowsAboutToBeInserted,
            this, [model, this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent);
        Q_ASSERT(last >= first);
        auto indexDiff = last - first;
        auto localFirst = localIndex(model, first);
        auto localLast = localFirst + indexDiff;
        beginInsertRows(QModelIndex(), localFirst, localLast);

        int count = indexDiff + 1;
        CachedRowCount += count;
    });

    connect(model, &cwKeywordModel::rowsInserted,
            this, [this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent);
        Q_UNUSED(first);
        Q_UNUSED(last);
        endInsertRows();
    });

    connect(model, &cwKeywordModel::rowsAboutToBeRemoved,
            this, [model, this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent);
        auto indexDiff = last - first;
        auto localFirst = localIndex(model, first);
        auto localLast = localFirst + indexDiff;
        beginRemoveRows(QModelIndex(), localFirst, localLast);

        int count = indexDiff + 1;
        CachedRowCount -= count;
    });

    connect(model, &cwKeywordModel::rowsRemoved,
            this, [this](const QModelIndex& parent, int first, int last)
    {
        Q_UNUSED(parent);
        Q_UNUSED(first);
        Q_UNUSED(last);
        endRemoveRows();
    });

    const int count = model->rowCount();
    const int firstIndex = rowCount();
    const int lastIndex = firstIndex + count - 1;

    Extentions.append(model);

    if(count > 0) {
        beginInsertRows(QModelIndex(), firstIndex, lastIndex);
        CachedRowCount += count;
        endInsertRows();
    }
}

void cwKeywordModel::removeExtension(cwKeywordModel *model)
{
    disconnect(model, nullptr, this, nullptr);

    int modelFirstIndex = Keywords.size();
    int lastCount = 0;
    QVector<cwKeywordModel*>::iterator keywordToRemoveIter = Extentions.end();
    for(auto iter = Extentions.begin(); iter != Extentions.end(); iter++) {
        auto currentModel = *iter;
        if(currentModel == model) {
            keywordToRemoveIter = iter;
        } else if (keywordToRemoveIter != Extentions.end()) {
            modelFirstIndex += currentModel->rowCount();
        } else {
            lastCount += currentModel->rowCount();
        }
    }

    int modelLastIndex = rowCount() - lastCount - 1;
    int count = modelLastIndex - modelFirstIndex + 1;

    if(modelFirstIndex <= modelLastIndex) {
        beginRemoveRows(QModelIndex(), modelFirstIndex, modelLastIndex);
        CachedRowCount -= count;
        Extentions.erase(keywordToRemoveIter);
        endRemoveRows();
    }
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

cwKeyword cwKeywordModel::createKeyword(const QString key, QString value) const
{
    return cwKeyword(key, value);
}

int cwKeywordModel::firstIndex(cwKeywordModel *model) const
{
    if(model == nullptr) {
        return 0;
    }

    Q_ASSERT(Extentions.contains(model));

    int extentionCount = Keywords.size();
    for(auto keywordModel : Extentions) {
        if(keywordModel == model) {
            return extentionCount;
        }
        extentionCount += keywordModel->rowCount();
    }
    return -1;
}

int cwKeywordModel::localIndex(cwKeywordModel *model, int index) const
{
    return firstIndex(model) + index;
}

std::pair<cwKeywordModel *, int> cwKeywordModel::modelAt(const QModelIndex &index) const
{
    int row = index.row() - Keywords.size();
    for(auto model : Extentions) {
        int currentCount = model->rowCount();
        if(row < currentCount) {
            return {model, row};
        }

        row -= currentCount;
    }

    return {nullptr, -1};
}

bool cwKeywordModel::canKeywordBeAdded(const cwKeyword &keyword)
{
    return !Keywords.contains(keyword) && keyword.isValid();
}

/**
 * Returns the number of keywords that are in the model
 */
int cwKeywordModel::rowCount(const QModelIndex &parent) const
{
    return CachedRowCount;
}

/**
 * Return all the keywords in the model
 */
QVector<cwKeyword> cwKeywordModel::keywords() const
{
    if(Extentions.isEmpty()) {
        return Keywords;
    }

    QVector<cwKeyword> keywords;
    keywords.reserve(rowCount());
    for(int i = 0; i < rowCount(); i++) {
        auto keyword = index(i).data(KeywordRole).value<cwKeyword>();
        keywords.append(keyword);
    }
    return keywords;
}
