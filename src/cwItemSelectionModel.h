#ifndef CWITEMSELECTIONMODEL_H
#define CWITEMSELECTIONMODEL_H

#include <QItemSelectionModel>

class cwItemSelectionModel : public QItemSelectionModel
{
    Q_OBJECT
public:
    explicit cwItemSelectionModel(QObject *parent = 0);
    explicit cwItemSelectionModel(QAbstractItemModel* model, QObject* parent = 0);
    
signals:
    
public slots:
    
};

#endif // CWITEMSELECTIONMODEL_H
