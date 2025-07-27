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
#include "cwGlobals.h"
#include "cwCavingRegionData.h"
class cwCavingRegion;

class CAVEWHERE_LIB_EXPORT cwRegionIOTask : public cwProjectIOTask
{
    Q_OBJECT
public:
    cwRegionIOTask(QObject* parent = nullptr);
    ~cwRegionIOTask();

    void setCavingRegion(const cwCavingRegionData& region);

    void copyRegionTo(cwCavingRegion *region);

    static int protoVersion();
    static QString toVersion(int protoVersion);

protected:
    cwCavingRegionData Region;

private:

};

#endif // CWREGIONIOTASK_H
