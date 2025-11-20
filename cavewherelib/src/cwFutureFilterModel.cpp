//Our includes
#include "cwFutureFilterModel.h"
#include "cwFutureManagerModel.h"

//Qt includes
#include <QAbstractItemModel>

cwFutureFilterModel::cwFutureFilterModel(QObject *parent) : QSortFilterProxyModel(parent)
{

}

/**
* delayTime is in ms
*/
void cwFutureFilterModel::setDelayTime(int delayTime) {
    if(DelayTime != delayTime) {
        DelayTime = delayTime;
        invalidateFilter();
        emit delayTimeChanged();
    }
}

bool cwFutureFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    auto modelIndex = sourceModel()->index(source_row, 0, source_parent);
    int elapsedTime = modelIndex.data(cwFutureManagerModel::RunTimeRole).toInt();
    return elapsedTime > DelayTime;
}
