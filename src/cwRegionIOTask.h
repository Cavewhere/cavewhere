/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWREGIONIOTASK_H
#define CWREGIONIOTASK_H

//Our includes
#include "cwProjectIOTask.h"
class cwCavingRegion;

class cwRegionIOTask : public cwProjectIOTask
{
    Q_OBJECT
public:
    cwRegionIOTask(QObject* parent = NULL);

    void setCavingRegion(const cwCavingRegion& region);

protected:
    cwCavingRegion* Region;
};

#endif // CWREGIONIOTASK_H
