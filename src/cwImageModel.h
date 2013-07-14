/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGEMODEL_H
#define CWIMAGEMODEL_H

#include <QAbstractItemModel>

class cwImageModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit cwImageModel(QObject *parent = 0);

signals:

public slots:


protected:


};

#endif // CWIMAGEMODEL_H
