/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTRINGLISTERRORMODEL_H
#define CWSTRINGLISTERRORMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QStringList>

class cwStringListErrorModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit cwStringListErrorModel(QObject *parent = 0);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    void setStringList(QStringList list);

signals:

public slots:

private:
    QStringList ErrorData;

};



#endif // CWSTRINGLISTERRORMODEL_H
