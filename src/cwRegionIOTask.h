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
    cwRegionIOTask(QObject* parent = nullptr);

    void setCavingRegion(const cwCavingRegion& region);

    void copyRegionTo(cwCavingRegion& region);

protected:
    cwCavingRegion* Region;

    static int version();

private:
//    void protectedMoveReginoToThread(QThread* thread);
//    Q_INVOKABLE void moveRegionToThread(QThread* thread);
//    Q_INVOKABLE void setRegionParent(QObject* parent);
};

#endif // CWREGIONIOTASK_H
