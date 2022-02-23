//

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
    mAcceptedModel(new QConcatenateTablesProxyModel(this)),
    mRejectedModel(new cwKeywordFilterModel(this))
{
    addRow();

    connect(mAcceptedModel, &cwKeywordItemModel::rowsInserted,
            this, [this](const QModelIndex& parent, int begin, int last)
    {
        Q_UNUSED(parent);
        for(int i = begin; i <= last; i++) {
            auto obj = mAcceptedModel->index(i, 0).data(cwKeywordItemModel::ObjectRole).value<QObject*>();
            Q_ASSERT(obj);
            mAccepted.insert(obj);
        }
    });

    connect(mAcceptedModel, &cwKeywordItemModel::rowsAboutToBeRemoved,
            this, [this](const QModelIndex& parent, int begin, int last)
    {
        Q_UNUSED(parent);
        for(int i = begin; i <= last; i++) {
            auto index = mAcceptedModel->index(i, 0);
            auto obj = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
            Q_ASSERT(obj);
            mAccepted.remove(obj);

            //Get the original index
            auto keywordModelIndex = [this](QModelIndex index) {
                while(index.model() != mKeywordModel) {
                    Q_ASSERT(dynamic_cast<const QAbstractProxyModel*>(index.model()));
                    auto model = static_cast<const QAbstractProxyModel*>(index.model());
                    index = model->mapToSource(index);
                }
                return index;
            }(mAcceptedModel->mapToSource(index));

            mRejectedModel->insert(keywordModelIndex);
        }
    });
}

void cwKeywordFilterPipelineModel::setKeywordModel(cwKeywordItemModel* keywordModel) {
    if(mKeywordModel != keywordModel) {
        if(mKeywordModel) {
            disconnect(mKeywordModel, nullptr, this, nullptr);
        }

        mKeywordModel = keywordModel;
        mRejectedModel->setSourceModel(keywordModel);
        linkFirst(0);

        if(mKeywordModel) {
            auto addToRejected = [this](int i) {
                auto index = mKeywordModel->index(i, 0, QModelIndex());
                auto obj = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                if(!mAccepted.contains(obj)) {
                    mRejectedModel->insert(index);
                }
            };

            connect(mKeywordModel, &cwKeywordItemModel::rowsInserted,
                    this, [addToRejected](const QModelIndex& parent, int begin, int last)
            {
                Q_UNUSED(parent);
                for(int i = begin; i <= last; i++) {
                    addToRejected(i);
                }
            });

            connect(mKeywordModel, &cwKeywordItemModel::dataChanged,
                    this, [addToRejected](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
            {
                if(roles.contains(cwKeywordItemModel::KeywordsRole)) {
                    for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
                        addToRejected(i);
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
        auto& row = mRows[index.row()];
        row.modelOperator = static_cast<Operator>(value.toInt());
        linkPipelineAt(index.row());
        emit dataChanged(index, index, {OperatorRole});
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

void cwKeywordFilterPipelineModel::insertRow(int i)
{
    i = std::max(0, std::min(mRows.size(), i));
    beginInsertRows(QModelIndex(), i, i);
    mRows.insert(i,
                 {
                     new cwKeywordGroupByKeyModel(this),
                     And
                 });

    //Update the pointers between the model's rows
    link(i);

    endInsertRows();
}

void cwKeywordFilterPipelineModel::addRow()
{
    insertRow(mRows.size());
}

void cwKeywordFilterPipelineModel::removeRow(int i)
{
    beginRemoveRows(QModelIndex(), i, i);
    const auto& row = mRows.at(i);
    mAcceptedModel->removeSourceModel(row.filter);
    row.filter->deleteLater();
    mRows.removeAt(i);
    link(i-1); //Update the pointers between model rows
    endInsertRows();
}

void cwKeywordFilterPipelineModel::link(int i)
{
    linkFirst(i);
    linkLast(i);
    linkPipelineAt(i);
    linkPipelineAt(i + 1);
}

void cwKeywordFilterPipelineModel::linkPipelineAt(int i)
{
    if(i < mRows.size()) {
        auto& row = mRows.at(i);

        switch(row.modelOperator) {
        case And:
            runAtRow(i - 1,
                     [&row, this](const Row& previousRow)
            {
                row.filter->setSourceModel(previousRow.filter->acceptedModel());
                mAcceptedModel->removeSourceModel(row.filter->acceptedModel());
            });
        case Or:
            runAtRow(i - 1,
                     [&row, this](const Row& previousRow)
            {
                //Add to AcceptedModel
                row.filter->setSourceModel(mKeywordModel); //All the data
                mAcceptedModel->addSourceModel(previousRow.filter->acceptedModel());
            });
        }
    }
}

void cwKeywordFilterPipelineModel::linkLast(int i)
{
    //Only operate on the last row
    if(i >= 0 && i == mRows.size() - 1) {
        runAtRow(i - 1,
                 [this](const Row& previousRow)
        {
            mAcceptedModel->removeSourceModel(previousRow.filter);
        });

        mAcceptedModel->addSourceModel(mRows.at(i).filter->acceptedModel());
    }
}

void cwKeywordFilterPipelineModel::linkFirst(int i)
{
    if(!mRows.isEmpty() && i == 0) {
        mRows.first().filter->setSourceModel(mKeywordModel);
    }
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

QAbstractItemModel *cwKeywordFilterPipelineModel::rejectedModel() const {
    return mRejectedModel;
}
