/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWQMLREGISTER_H
#define CWQMLREGISTER_H

//Our includes
#include "cwGlobals.h"
#include "cwCave.h"

//Qt includes
#include <QList>
#include <QGraphicsScene>

class CAVEWHERE_LIB_EXPORT cwQMLRegister
{
public:
    cwQMLRegister();

    // static void registerQML();
};

// typedef QList<cwCave*> CaveList;

class CaveListRegistration
{
    Q_GADGET
    QML_FOREIGN(QList<cwCave*>)
    QML_ANONYMOUS
    QML_SEQUENTIAL_CONTAINER(cwCave*)
};

class cwGraphicsScene : public QGraphicsScene {
    Q_OBJECT
    QML_FOREIGN(QGraphicsScene)
    QML_ANONYMOUS
};

#endif // CWQMLREGISTER_H
