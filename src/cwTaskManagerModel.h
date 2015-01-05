/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWTASKMANAGERMODEL_H
#define CWTASKMANAGERMODEL_H

#include <QAbstractListModel>

class cwTaskManagerModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit cwTaskManagerModel(QObject *parent = 0);

signals:

public slots:

};

#endif // CWTASKMANAGERMODEL_H
