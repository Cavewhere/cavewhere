#ifndef CWTASKFUTURECOMBINEMODEL_H
#define CWTASKFUTURECOMBINEMODEL_H

//Qt includes
#include <QConcatenateTablesProxyModel>
#include <QQmlEngine>

#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwTaskFutureCombineModel : public QConcatenateTablesProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TaskFutureCombineModel)
    Q_PROPERTY(QList<QObject*> models READ models WRITE setModels NOTIFY modelsChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    cwTaskFutureCombineModel(QObject* parent = nullptr);

    int count() const;

    QList<QObject*> models() const;
    void setModels(QList<QObject*> models);

    QHash<int, QByteArray> roleNames() const;

signals:
    void modelsChanged();
    void countChanged();

private:
    QList<QObject*> Models; //!<
};

inline QList<QObject*> cwTaskFutureCombineModel::models() const {
    return Models;
}

#endif // CWTASKFUTURECOMBINEMODEL_H
