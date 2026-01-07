//Std includse
#include <algorithm>

//Our includes
#include "cwKeywordFilterPipelineModel.h"
#include "cwKeywordFilterModel.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordGroupByKeyModel.h"
#include "cwUniqueValueFilterModel.h"
#include "cwDebug.h"

cwKeywordFilterPipelineModel::cwKeywordFilterPipelineModel(QObject *parent) :
    QAbstractListModel(parent),
    mAcceptedModel(new QConcatenateTablesProxyModel(this)),
    mUniqueAcceptedModel(new cwUniqueValueFilterModel(this)),
    mRejectedModel(new cwKeywordFilterModel(this))
{
    mUniqueAcceptedModel->setSourceModel(mAcceptedModel);
    mUniqueAcceptedModel->setUniqueRole(cwKeywordItemModel::ObjectRole);
    mUniqueAcceptedModel->setLessThan([](const QVariant& left, const QVariant& right) {
        return left.value<QObject*>() < right.value<QObject*>();
    });

    addRow();

    auto findKeywordModelIndex = [this]( QModelIndex index) {
        while(index.model() != mKeywordModel) {
            if(auto proxyModel = dynamic_cast<const QAbstractProxyModel*>(index.model())) {
                index = proxyModel->mapToSource(index);
            } else if(auto concatModel = dynamic_cast<const QConcatenateTablesProxyModel*>(index.model())) {
                index = concatModel->mapToSource(index);
            } else {
                Q_ASSERT(false);
                break;
            }
        }
        return index;
    };

    auto validateUniqueModel = [this, findKeywordModelIndex](const char* context) {
#ifdef QT_DEBUG
        for(int row = 0; row < mUniqueAcceptedModel->rowCount(); ++row) {
            const auto proxyIndex = mUniqueAcceptedModel->index(row, 0);
            Q_ASSERT(proxyIndex.isValid());

            const auto sourceIndex = mUniqueAcceptedModel->mapToSource(proxyIndex);
            Q_ASSERT(sourceIndex.isValid());
            // qDebug() << "Sources:" << mAcceptedModel->sourceModels() << sourceIndex.model() << proxyIndex.model();
            // Q_ASSERT(mAcceptedModel->sourceModels().contains(sourceIndex.model()));

            const auto obj = proxyIndex.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
            if(!obj) {
                qWarning() << context << "null object at unique row" << row << proxyIndex << sourceIndex;
            }
            Q_ASSERT(obj);

            const auto keywordIndex = findKeywordModelIndex(proxyIndex);
            if(!keywordIndex.isValid()) {
                qWarning() << context << "failed to map back to keyword model from" << proxyIndex;
            }
            Q_ASSERT(keywordIndex.isValid());
        }
#endif
    };

    connect(mUniqueAcceptedModel, &cwKeywordItemModel::rowsInserted,
            this, [this, findKeywordModelIndex, validateUniqueModel](const QModelIndex& parent, int begin, int last)
            {
                Q_UNUSED(parent);
                for(int i = begin; i <= last; i++) {
                    auto index = mUniqueAcceptedModel->index(i, 0);
                    auto obj = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                    Q_ASSERT(obj);
                    auto keywordIndex = findKeywordModelIndex(index);
                    mRejectedModel->remove(keywordIndex);
                    // qDebug() << "Remove from rejected:" << keywordIndex << keywordIndex.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                }
                validateUniqueModel("rowsInserted");
            });

    connect(mUniqueAcceptedModel, &cwKeywordItemModel::rowsAboutToBeRemoved,
            this, [this, findKeywordModelIndex, validateUniqueModel](const QModelIndex& parent, int begin, int last)
            {
                Q_UNUSED(parent);
                // qDebug() << "Rows about to be removed:" << begin << last;
                for(int i = begin; i <= last; i++) {
                    auto index = mUniqueAcceptedModel->index(i, 0);
                    Q_ASSERT(index.isValid());

                    // qDebug() << "Unique model:" << mUniqueAcceptedModel->rowCount() << i;

                    auto obj = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                    if(obj) {
                        //Get the original index
                        auto keywordModelIndex = findKeywordModelIndex(index);

                        // qDebug() << "adding to rejected:" << index << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                        mRejectedModel->insert(keywordModelIndex);
                    }
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
            auto indexObj = [this](int i) {
                const auto index = mKeywordModel->index(i, 0, QModelIndex());
                return std::tuple(index,
                                  index.data(cwKeywordItemModel::ObjectRole).value<QObject*>());
            };

            auto addToRejected = [this, indexObj](int i) {
                auto [index, obj] = indexObj(i);
                // qDebug() << "try to add to rejected:" << index << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                if(!mUniqueAcceptedModel->contains(QVariant::fromValue(obj))) {
                    // qDebug() << "adding to rejected from keywordModel:" << index << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                    mRejectedModel->insert(index);
                }
            };

            auto removeFromRejected = [this, indexObj](int i) {
                auto [index, obj] = indexObj(i);
                // qDebug() << "try to remove from rejected:" << index << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                // qDebug() << "removing from rejected from keywordModel:" << mRejectedModel << index << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                mRejectedModel->remove(index);
            };

            connect(mKeywordModel, &cwKeywordItemModel::rowsInserted,
                    this, [addToRejected, this](const QModelIndex& parent, int begin, int last)
                    {
                        Q_UNUSED(parent);
                        if(parent == QModelIndex()) {
                            for(int i = begin; i <= last; i++) {
                                addToRejected(i);
                            }
                            updatePossibleKeys();
                        }
                    });

            connect(mKeywordModel, &cwKeywordItemModel::rowsAboutToBeRemoved,
                    this, [this, removeFromRejected](const QModelIndex& parent, int begin, int last)
                    {
                        if(parent == QModelIndex()) {
                            for(int i = begin; i <= last; i++) {
                                removeFromRejected(i);
                            }
                            updatePossibleKeys();
                        }
                    });

            connect(mKeywordModel, &cwKeywordItemModel::dataChanged,
                    this, [this, addToRejected](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
                    {
                        if(roles.contains(cwKeywordItemModel::KeywordsRole)) {
                            if(topLeft.parent().isValid()) {
                                addToRejected(topLeft.parent().row());
                            } else {
                                for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
                                    addToRejected(i);
                                }
                            }
                            updatePossibleKeys();
                        }
                    });
        }

        updatePossibleKeys();
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
        auto newOperator = static_cast<Operator>(value.toInt());
        if(newOperator != row.modelOperator) {
            row.modelOperator = newOperator;

            if(newOperator == Or) {
                // New OR groups should start with everything unchecked.
                row.filter->setAcceptedByDefault(false);
                for(int i = 0; i < row.filter->rowCount(); i++) {
                    const auto filterIndex = row.filter->index(i, 0);
                    row.filter->setData(filterIndex, false, cwKeywordGroupByKeyModel::AcceptedRole);
                }
            } else {
                row.filter->setAcceptedByDefault(true);
            }

            linkPipelineAt(index.row());
            emit dataChanged(index, index, {OperatorRole});
            return true;
        }
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
    i = std::max(0, std::min((int)mRows.size(), i));
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
    if(i < 0 || i >= mRows.size()) {
        qWarning() << "Invalid removeRow index" << i << "rows:" << mRows.size();
        return;
    }

    const bool removedWasOr = mRows.at(i).modelOperator == Or;

    beginRemoveRows(QModelIndex(), i, i);
    const auto& row = mRows.at(i);
    // Q_ASSERT();

    // for(auto sourceModel : mAcceptedModel->sourceModels()) {
    //     qDebug() << "SourceModel:" << sourceModel << row.filter->acceptedModel() << (sourceModel == row.filter->acceptedModel());
    // }

    if(mAcceptedModel->sourceModels().contains(row.filter->acceptedModel())) {
        mAcceptedModel->removeSourceModel(row.filter->acceptedModel());
    } else {
        qWarning() << "Can't remove " << row.filter->acceptedModel() << "this is a bug" << LOCATION;
    }
    row.filter->deleteLater();
    mRows.removeAt(i);
    if(i == 0) {
        linkFirst(0);
    }

    linkPipelineAt(i - 1);
    linkPipelineAt(i);

    if(i == mRows.size()) {
        linkLast(mRows.size() - 1);
    }

    // link(i-1); //Update the pointers between model rows
    endRemoveRows();

    // Preserve the OR boundary when removing the first row of an OR group by
    // promoting the following row to OR so the number of groups stays stable.
    if(removedWasOr) {
        auto nextIndex = index(i);
        if(nextIndex.isValid()) {
            setData(nextIndex, Or, OperatorRole);
        }
    }
}

void cwKeywordFilterPipelineModel::link(int i)
{
    linkFirst(i);
    linkLast(i);
    // qDebug() << "Pipeline: " << i;
    linkPipelineAt(i);
    // qDebug() << "Pipeline: + 1";
    linkPipelineAt(i + 1);
}

void cwKeywordFilterPipelineModel::linkPipelineAt(int i)
{
    if(i >= 0 && i < mRows.size()) {
        auto& row = mRows.at(i);

        switch(row.modelOperator) {
        case And:
            runAtRow(i - 1,
                     [&row, this](const Row& previousRow)
                     {
                         row.filter->setSourceModel(previousRow.filter->acceptedModel());
                         if(mAcceptedModel->sourceModels().contains(previousRow.filter->acceptedModel())) {
                             mAcceptedModel->removeSourceModel(previousRow.filter->acceptedModel());
                             // qDebug() << "removed pipeline:" << row.filter->acceptedModel() << mAcceptedModel->sourceModels();
                         }
                     });
            break;
        case Or:
            runAtRow(i - 1,
                     [&row, this](const Row& previousRow)
                     {
                         //Add to AcceptedModel
                         // qDebug() << "start add pipeline";
                         row.filter->setSourceModel(mKeywordModel); //All the data
                         if(!mAcceptedModel->sourceModels().contains(previousRow.filter->acceptedModel())) {
                             mAcceptedModel->addSourceModel(previousRow.filter->acceptedModel());
                         }
                         // qDebug() << "added pipeline:" << previousRow.filter->acceptedModel() << mAcceptedModel->sourceModels();
                     });
            break;
        }
    }
}

void cwKeywordFilterPipelineModel::linkLast(int i)
{
    //Only operate on the last row
    if(!mRows.isEmpty() && i == mRows.size() - 1) {
        runAtRow(i - 1,
                 [this](const Row& previousRow)
                 {
                     Q_ASSERT(previousRow.filter->acceptedModel());
                     if(mAcceptedModel->sourceModels().contains(previousRow.filter->acceptedModel())) {
                         mAcceptedModel->removeSourceModel(previousRow.filter->acceptedModel());
                         // qDebug() << "removed linkLast:" << previousRow.filter->acceptedModel() << mAcceptedModel->sourceModels();
                     }
                 });

        // qDebug() << "adding linkLast:" << mRows.at(i).filter->acceptedModel() << "existing models:" << mAcceptedModel->sourceModels();
        // qDebug() << "mRows.at(i).filter" << mRows.at(i).filter->acceptedModel();
        // for(int j = 0; j < mRows.at(i).filter->acceptedModel()->rowCount(); j++) {
        //     auto index = mRows.at(i).filter->acceptedModel()->index(j);;
        //     qDebug() << "Index:" << index << index.isValid();
        // }

        if(!mAcceptedModel->sourceModels().contains(mRows.at(i).filter->acceptedModel())) {
            mAcceptedModel->addSourceModel(mRows.at(i).filter->acceptedModel());
        }

    }
}

void cwKeywordFilterPipelineModel::linkFirst(int i)
{
    if(!mRows.isEmpty() && i <= 0) {
        mRows.first().filter->setSourceModel(mKeywordModel);
    }
}

void cwKeywordFilterPipelineModel::updatePossibleKeys()
{
    QSet<QString> keys;
    for(int i = 0; i < mKeywordModel->rowCount(); i++) {
        auto index = mKeywordModel->index(i, 0, QModelIndex());
        const auto keywords = index.data(cwKeywordItemModel::KeywordsRole).value<QVector<cwKeyword>>();
        for(const auto& keyword : keywords) {
            keys.insert(keyword.key());
        }
    }
    mPossibleKeys = QStringList(keys.begin(), keys.end());
    std::sort(mPossibleKeys.begin(), mPossibleKeys.end());
    emit possibleKeysChanged();
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

QAbstractItemModel *cwKeywordFilterPipelineModel::acceptedModel() const {
    return mUniqueAcceptedModel;
}

void cwKeywordFilterPipelineModel::clear()
{
    beginResetModel();
    mRows.clear();

    const auto sourceModels = mAcceptedModel->sourceModels();
    for(auto model : sourceModels) {
        mAcceptedModel->removeSourceModel(model);
    }
    Q_ASSERT(mAcceptedModel->sourceModels().isEmpty());
    Q_ASSERT(mAcceptedModel->rowCount() == 0);

    // mAcceptedModel->clear();
    // mUniqueAcceptedModel->clear();
    mRejectedModel->clear();
    addRow();
    endResetModel();
    updatePossibleKeys();
}
