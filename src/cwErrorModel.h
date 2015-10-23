/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWERRORMANAGER_H
#define CWERRORMANAGER_H

#include <QObject>
#include <QHash>
#include <QSet>
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
    void removeErrorsFor(QObject* parent);

    void addParent(const QObject* parent);

    QVariantList errors(const QObject* parent) const;
    QVariantList errors(const QObject* parent, int index) const;
    QVariantList errors(const QObject* parent, int index, int role) const;

signals:
    void parentErrorsChanged(QObject* parent);
    void errorsChanged(QObject* parent, int index, int role);

public slots:

private:
    QHash<const QObject*, QList<cwError>> Database; //List of errors for each parent
    QSet<const QObject*> Parents;

    void emitErrorChanged(QObject* parent);

};

#endif // CWERRORMANAGER_H
