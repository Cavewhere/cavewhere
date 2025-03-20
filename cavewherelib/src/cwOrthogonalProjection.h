/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWORTHOGONALPROJECTION_H
#define CWORTHOGONALPROJECTION_H

//Qt includes
#include <QQmlEngine>

//Our includes
#include "cwAbstractProjection.h"
class cwLength;

class cwOrthogonalProjection : public cwAbstractProjection
{
    Q_OBJECT
    QML_NAMED_ELEMENT(OrothogonalProjection)

public:
    explicit cwOrthogonalProjection(QObject *parent = 0);

protected:
    cwProjection calculateProjection();
    
signals:
    
public slots:

private:
};



#endif // CWORTHOGONALPROJECTION_H
