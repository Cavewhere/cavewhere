#include "cwTaskFutureCombineModel.h"
#include "cwFutureManagerModel.h"

cwTaskFutureCombineModel::cwTaskFutureCombineModel(QObject *parent)
    : QConcatenateTablesProxyModel(parent)
{

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
