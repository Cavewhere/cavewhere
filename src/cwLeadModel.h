/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEADMODEL_H
#define CWLEADMODEL_H

#include <QObject>

class cwLeadModel : public QObject
{
    Q_OBJECT
public:
    explicit cwLeadModel(QObject *parent = 0);
    ~cwLeadModel();

signals:

public slots:
};

#endif // CWLEADMODEL_H
