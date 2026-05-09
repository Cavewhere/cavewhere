#ifndef CWCAVINGREGIONDATA_H
#define CWCAVINGREGIONDATA_H

#include "cwCaveData.h"
#include "cwGeoPoint.h"

//This is useful for async thread process, extracts all the data from cwCavingRegion
struct cwCavingRegionData {
    QString name;
    QList<cwCaveData> caves;
    QString globalCS;
    cwGeoPoint worldOrigin;
};


#endif // CWCAVINGREGIONDATA_H
