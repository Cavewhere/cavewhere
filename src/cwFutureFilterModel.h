#ifndef CWFUTUREFILTERMODEL_H
#define CWFUTUREFILTERMODEL_H

//Qt includes
#include <QSortFilterProxyModel>

//Our inculdes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwFutureFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(int delayTime READ delayTime WRITE setDelayTime NOTIFY delayTimeChanged)

public:
    explicit cwFutureFilterModel(QObject *parent = nullptr);

    int delayTime() const;
    void setDelayTime(int delayTime);

signals:
    void delayTimeChanged();

public slots:

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
    int DelayTime = 1500; //!<
};

/**
* Returns the delay time for the filter in ms
*/
inline int cwFutureFilterModel::delayTime() const {
    return DelayTime;
}

#endif // CWFUTUREFILTERMODEL_H
