//Our includes
#include "cwKeywordFilterPipelineModel.h"
#include "cwKeywordFilterModel.h"
#include "cwKeywordItemModel.h"
#include "cwEntityKeywordsModel.h"
#include "cwKeywordGroupByKeyModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"

cwKeywordFilterPipelineModel::cwKeywordFilterPipelineModel(QObject *parent) :
    QAbstractListModel(parent),
    mEntityKeywords(new cwEntityKeywordsModel(this))
{

}

void cwKeywordFilterPipelineModel::setKeywordModel(cwKeywordItemModel* keywordModel) {
    if(mKeywordModel != keywordModel) {

        if(mKeywordModel) {
            disconnect(mKeywordModel, nullptr, this, nullptr);
        }

        mKeywordModel = keywordModel;

        if(mKeywordModel) {
            connect(mKeywordModel, &cwKeywordItemModel::rowsInserted,
                    this, [this](const QModelIndex& parent, int first, int last)
            {
                if(parent == QModelIndex()) {
                    for(int i = first; i <= last; i++) {
                        auto item = mKeywordModel->item(i);
                        mEntityKeywords->insert({item->object(),
                                                 item->keywordModel()->keywords()});
                    }
                }

            });

            connect(mKeywordModel, &cwKeywordItemModel::dataChanged,
                    this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
            {
                if(roles.contains(cwKeywordItemModel::KeywordsRole)
                        && topLeft.parent() == QModelIndex()
                        && bottomRight.parent() == QModelIndex())
                {

                    for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
                        auto item = mKeywordModel->item(i);
                        mEntityKeywords->insert({item->object(),
                                                 item->keywordModel()->keywords()});

                    }
                }
            });

            connect(mKeywordModel, &cwKeywordItemModel::rowsAboutToBeRemoved,
                    this, [this](const QModelIndex& parent, int first, int last)
            {
                if(parent == QModelIndex()) {
                    for(int i = first; i <= last; i++) {
                        auto item = mKeywordModel->item(i);
                        mEntityKeywords->remove(item->object());
                    }
                }
            });
        }

        emit keywordModelChanged();
    }
}

int cwKeywordFilterPipelineModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return mRows.size();
}

QVariant cwKeywordFilterPipelineModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    const auto& row = mRows.at(index.row());

    switch(role) {
    case FilterModelObjectRole:
        return QVariant::fromValue(row.filter);
    case OperatorRole:
        return row.modelOperator;
    default:
        return QVariant();
    }

    return QVariant();
}

bool cwKeywordFilterPipelineModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid()) {
        return false;
    }

    if(role == OperatorRole) {

    }

    return false;

}

QHash<int, QByteArray> cwKeywordFilterPipelineModel::roleNames() const
{
    static const QHash<int, QByteArray> names({
                                                  {FilterModelObjectRole, "filterModelObjectRole"},
                                                  {OperatorRole, "operatorRole"}
                                              });
    return names;
}

QVector<cwEntityAndKeywords> cwKeywordFilterPipelineModel::acceptedEntities() const
{
    return {};
}

cwKeywordItemModel *cwKeywordFilterPipelineModel::keywordModel() const {
    return mKeywordModel;
}

QStringList cwKeywordFilterPipelineModel::operators() const {
    return {
        "Or"
        "And"
    };
}
