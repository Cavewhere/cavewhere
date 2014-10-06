/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWCAPTUREGROUPMODEL_H
#define CWCAPTUREGROUPMODEL_H

#include <QAbstractItemModel>

class cwCaptureGroupModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit cwCaptureGroupModel(QObject *parent = 0);

signals:

public slots:

};

#endif // CWCAPTUREGROUPMODEL_H
