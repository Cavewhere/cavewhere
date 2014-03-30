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
class cwLength;

class cwOrthogonalProjection : public cwAbstractProjection
{
    Q_OBJECT

public:
    explicit cwOrthogonalProjection(QObject *parent = 0);

protected:
    cwProjection calculateProjection();
    
signals:
    
public slots:

private:
};



#endif // CWORTHOGONALPROJECTION_H
