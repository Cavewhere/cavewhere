/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWERRORMANAGER_H
#define CWERRORMANAGER_H

#include <QObject>
#include <QAbstractListModel>

//Our include
#include "cwError.h"

class cwErrorModel : public QObject
{
    Q_OBJECT
public:
    explicit cwErrorModel(QObject *parent = 0);

    void addError(const cwError& error);
    void removeError(const cwError& error);

    QVariantList errors(const QObject* parent) const;
    QVariantList errors(const QObject* parent, int index) const;

signals:
    void errorsChanged(const QObject* parent);

public slots:

private:
    QHash<QObject*, QList<cwError>> Database; //List of errors for each parent

    void emitErrorChanged(const QObject* parent);
};

#endif // CWERRORMANAGER_H
