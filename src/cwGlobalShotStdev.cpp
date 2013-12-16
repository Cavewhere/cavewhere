/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwGlobalShotStdev.h"

cwGlobalShotStdev::cwGlobalShotStdev(QObject *parent) :
    QObject(parent)
{
}

/**
Sets distanceStd
*/
void cwGlobalShotStdev::setDistanceStd(double distanceStd) {
    if(distanceStd > 0) {
        if(Data.DistanceStd != distanceStd) {
            Data.DistanceStd = distanceStd;
            emit distanceStdChanged();
        }
    }
}

/**
Sets clinoStd
*/
void cwGlobalShotStdev::setClinoStd(double clinoStd) {
    if(clinoStd > 0) {
        if(Data.ClinoStd != clinoStd) {
            Data.ClinoStd = clinoStd;
            emit clinoStdChanged();
        }
    }
}

/**
Sets compassStd
*/
void cwGlobalShotStdev::setCompassStd(double compassStd) {
    if(compassStd > 0) {
        if(Data.CompassStd != compassStd) {
            Data.CompassStd = compassStd;
            emit compassStdChanged();
        }
    }
}

cwShotStdev cwGlobalShotStdev::data() const
{
    return Data;
}

void cwGlobalShotStdev::setData(cwShotStdev newData)
{
    setDistanceStd(newData.DistanceStd);
    setClinoStd(newData.ClinoStd);
    setCompassStd(newData.CompassStd);
}
