// Our includes
#include "cwKeywordFilterGroupProxyModel.h"

cwKeywordFilterGroupProxyModel::cwKeywordFilterGroupProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

int cwKeywordFilterGroupProxyModel::groupIndex() const
{
    return m_groupIndex;
}

void cwKeywordFilterGroupProxyModel::setGroupIndex(int groupIndex)
{
    if(m_groupIndex == groupIndex) {
        return;
    }
    m_groupIndex = groupIndex;
    emit groupIndexChanged();
    invalidateRowsFilter();
    // emitDerivedDataChanged();
}

QVariant cwKeywordFilterGroupProxyModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    const auto sourceIdx = mapToSource(index);
    if(!sourceIdx.isValid()) {
        return QVariant();
    }

    switch(role) {
    case SourceRowRole:
        return sourceIdx.row();
    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

QHash<int, QByteArray> cwKeywordFilterGroupProxyModel::roleNames() const
{
    auto names = QSortFilterProxyModel::roleNames();
    names.insert(SourceRowRole, "sourceRow");
    return names;
}

bool cwKeywordFilterGroupProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent)
    return groupIndexForRow(sourceRow) == m_groupIndex;
}

void cwKeywordFilterGroupProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    auto* current = this->sourceModel();
    if(current) {
        disconnect(current, nullptr, this, nullptr);
    }

    QSortFilterProxyModel::setSourceModel(sourceModel);

    if(!sourceModel) {
        return;
    }

    // connect(sourceModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& parent, int first, int last) {
    //     Q_UNUSED(parent)
    //     Q_UNUSED(first)
    //     Q_UNUSED(last)
    //     // emitDerivedDataChanged();
    // });

    connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this, sourceModel](const QModelIndex& parent, int first, int last) {
        Q_UNUSED(parent)
        Q_UNUSED(first)
        Q_UNUSED(last)

        for(int i = first; i <= last; i++) {
            if(sourceModel->index(i, 0)
                    .data(cwKeywordFilterPipelineModel::OperatorRole)
                    .toInt()
                == cwKeywordFilterPipelineModel::Operator::Or) {
                shouldInvalidate = true;
                return;
            }
        }
    });

    connect(sourceModel, &QAbstractItemModel::rowsRemoved, this, [this, sourceModel](const QModelIndex& parent, int first, int last) {
        if(shouldInvalidate) {
            invalidateRowsFilter();
            shouldInvalidate = false;
        }
    });

    connect(sourceModel, &QAbstractItemModel::modelReset, this, [this]() {
        invalidateFilter();
    });
    connect(sourceModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& topLeft,
                                                                        const QModelIndex& bottomRight,
                                                                        const QVector<int>& roles)
    {
        Q_UNUSED(topLeft)
        Q_UNUSED(bottomRight)
        const bool opChanged = roles.isEmpty() || roles.contains(cwKeywordFilterPipelineModel::OperatorRole);
        if(opChanged) {
            invalidateRowsFilter();
        }
    });
}

int cwKeywordFilterGroupProxyModel::operatorAt(int row) const
{
    auto* source = sourceModel();
    return source->index(row, 0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt();
}


int cwKeywordFilterGroupProxyModel::groupIndexForRow(int sourceRow) const
{
    if(sourceRow < 0) {
        return -1;
    }

    int groupIndex = -1;
    auto* source = sourceModel();
    const int count = source ? source->rowCount() : 0;
    //This is fairly slow functinon, but since count will always be less than 10
    //it's not that bad.
    for(int row = 0; row <= sourceRow && row < count; ++row) {
        if(row == 0 || operatorAt(row) == cwKeywordFilterPipelineModel::Or) {
            ++groupIndex;
        }
    }
    return groupIndex;
}
