#include "cwTaskFutureCombineModel.h"
#include "cwFutureManagerModel.h"

//Std includes
#include <algorithm>

namespace {
    //! Nothing can say how far along it is, so there is no honest number to give
    constexpr double kIndeterminateProgress = -1.0;
}

cwTaskFutureCombineModel::cwTaskFutureCombineModel(QObject *parent)
    : QConcatenateTablesProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &cwTaskFutureCombineModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved,  this, &cwTaskFutureCombineModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset,   this, &cwTaskFutureCombineModel::countChanged);

    connect(this, &cwTaskFutureCombineModel::countChanged, this, &cwTaskFutureCombineModel::progressChanged);

    //Adding a source model publishes its rows before its columns, and with no
    //columns there are no cells to read — so a source that arrives with jobs
    //already running reads as indeterminate until the columns land. That is what
    //announces the real number.
    connect(this, &QAbstractItemModel::columnsInserted, this, &cwTaskFutureCombineModel::progressChanged);
    connect(this, &QAbstractItemModel::columnsRemoved,  this, &cwTaskFutureCombineModel::progressChanged);

    //The future manager retimes every row four times a second just to refresh
    //the elapsed clock, so an unfiltered dataChanged would republish progress
    //that hasn't moved.
    connect(this, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
        if(roles.isEmpty()
                || roles.contains(cwFutureManagerModel::ProgressRole)
                || roles.contains(cwFutureManagerModel::NumberOfStepRole)) {
            emit progressChanged();
        }
    });
}

int cwTaskFutureCombineModel::count() const
{
    return rowCount();
}

/**
 * Averages the jobs that can say how far along they are, ignoring the ones that
 * can't. Each measured job counts once: a point cloud's steps are points and a
 * scrap's are outlines, so weighting by step count would just let whichever job
 * chose the finer unit decide the whole number.
 */
double cwTaskFutureCombineModel::progress() const
{
    //A source model part-way through being added has rows but no columns yet, so
    //there are no cells to read — see the constructor.
    if(columnCount() <= 0) {
        return kIndeterminateProgress;
    }

    const int rows = rowCount();
    double sum = 0.0;
    int measured = 0;

    for(int i = 0; i < rows; i++) {
        const QModelIndex rowIndex = index(i, 0);
        const int steps = rowIndex.data(cwFutureManagerModel::NumberOfStepRole).toInt();

        if(steps <= 0) {
            continue;
        }

        const int completed = rowIndex.data(cwFutureManagerModel::ProgressRole).toInt();
        sum += std::clamp(static_cast<double>(completed) / steps, 0.0, 1.0);
        measured++;
    }

    return measured > 0 ? sum / measured : kIndeterminateProgress;
}

QHash<int, QByteArray> cwTaskFutureCombineModel::roleNames() const
{
    return cwFutureManagerModel::defaultRoles();
}

/**
*
*/
void cwTaskFutureCombineModel::setModels(QList<QObject*> models) {
    if(Models != models) {
        for(auto objModel : Models) {
            Q_ASSERT(dynamic_cast<QAbstractItemModel*>(objModel));
            auto model = static_cast<QAbstractItemModel*>(objModel);
            removeSourceModel(model);
        }

        Models.clear();
        Models.reserve(models.size());

        for(auto objModel : models) {
            auto model = dynamic_cast<QAbstractItemModel*>(objModel);
            if(model) {
                addSourceModel(model);
                Models.append(model);
            }
        }

        emit modelsChanged();
    }
}
