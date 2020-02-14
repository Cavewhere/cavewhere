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
class cwCavingRegion;

class CAVEWHERE_LIB_EXPORT cwRegionIOTask : public cwProjectIOTask
{
    Q_OBJECT
public:
    cwRegionIOTask(QObject* parent = nullptr);

    void setCavingRegion(const cwCavingRegion& region);

    void copyRegionTo(cwCavingRegion& region);

    static int protoVersion();
    static QString toVersion(int protoVersion);

protected:
    cwCavingRegion* Region;

private:

};

#endif // CWREGIONIOTASK_H
