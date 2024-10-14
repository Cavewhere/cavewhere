#ifndef CWTASKFUTURECOMBINEMODEL_H
#define CWTASKFUTURECOMBINEMODEL_H

//Qt includes
#include <QConcatenateTablesProxyModel>
#include <QQmlEngine>

class cwTaskFutureCombineModel : public QConcatenateTablesProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TaskFutureCombineModel)
    Q_PROPERTY(QList<QObject*> models READ models WRITE setModels NOTIFY modelsChanged)

public:
    cwTaskFutureCombineModel(QObject* parent = nullptr);

    QList<QObject*> models() const;
    void setModels(QList<QObject*> models);

    QHash<int, QByteArray> roleNames() const;

signals:
    void modelsChanged();

private:
    QList<QObject*> Models; //!<
};

inline QList<QObject*> cwTaskFutureCombineModel::models() const {
    return Models;
}

#endif // CWTASKFUTURECOMBINEMODEL_H
