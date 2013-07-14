/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWORTHOGONALPROJECTION_H
#define CWORTHOGONALPROJECTION_H

//Our includes
#include "cwAbstractProjection.h"

class cwOrthogonalProjection : public cwAbstractProjection
{
    Q_OBJECT

public:
    explicit cwOrthogonalProjection(QObject *parent = 0);

protected:
    cwProjection calculateProjection();
    
signals:
    
public slots:
    
};

#endif // CWORTHOGONALPROJECTION_H
