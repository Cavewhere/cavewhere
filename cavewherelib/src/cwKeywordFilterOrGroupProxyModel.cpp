// Std includes
#include <algorithm>

// Our includes
#include "cwKeywordFilterOrGroupProxyModel.h"

cwKeywordFilterOrGroupProxyModel::cwKeywordFilterOrGroupProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    // setDynamicSortFilter(true);
}

// QVariant cwKeywordFilterOrGroupProxyModel::data(const QModelIndex& index, int role) const
// {
//     if(!index.isValid()) {
//         return QVariant();
//     }

//     const auto sourceIdx = mapToSource(index);
//     if(!sourceIdx.isValid()) {
//         return QVariant();
//     }

//     return QSortFilterProxyModel::data(index, role);
// }

// QHash<int, QByteArray> cwKeywordFilterOrGroupProxyModel::roleNames() const
// {
//     return QSortFilterProxyModel::roleNames();
// }

bool cwKeywordFilterOrGroupProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent)
    if(sourceRow == 0) {
        return true;
    }
    int operatorRole = sourceModel()
                           ->index(sourceRow, 0)
                           .data(cwKeywordFilterPipelineModel::OperatorRole)
                           .toInt();
    return operatorRole == cwKeywordFilterPipelineModel::Or;
}

// void cwKeywordFilterOrGroupProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
// {
//     auto* current = this->sourceModel();
//     if(current) {
//         disconnect(current, nullptr, this, nullptr);
//     }

//     QSortFilterProxyModel::setSourceModel(sourceModel);

//     if(!sourceModel) {
//         return;
//     }

//     connect(sourceModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& parent, int first, int last) {
//         Q_UNUSED(parent)
//         Q_UNUSED(first)
//         Q_UNUSED(last)
//         emitDerivedDataChanged();
//     });
//     connect(sourceModel, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex& parent, int first, int last) {
//         Q_UNUSED(parent)
//         Q_UNUSED(first)
//         Q_UNUSED(last)
//         emitDerivedDataChanged();
//     });
//     connect(sourceModel, &QAbstractItemModel::modelReset, this, [this]() {
//         invalidateFilter();
//         emitDerivedDataChanged();
//     });
//     connect(sourceModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& topLeft,
//                                                                         const QModelIndex& bottomRight,
//                                                                         const QVector<int>& roles)
//     {
//         Q_UNUSED(topLeft)
//         Q_UNUSED(bottomRight)
//         const bool opChanged = roles.isEmpty() || roles.contains(cwKeywordFilterPipelineModel::OperatorRole);
//         if(opChanged) {
//             invalidateFilter();
//         }
//         emitDerivedDataChanged();
//     });
// }

// int cwKeywordFilterOrGroupProxyModel::operatorAt(int row) const
// {
//     auto* src = sourceModel();
//     if(!src || row < 0 || row >= src->rowCount()) {
//         return cwKeywordFilterPipelineModel::And;
//     }
//     return src->index(row, 0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt();
// }


// void cwKeywordFilterOrGroupProxyModel::emitDerivedDataChanged()
// {
//     if(rowCount() == 0) {
//         return;
//     }
//     emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
// }
