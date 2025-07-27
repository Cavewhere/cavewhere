#ifndef CWCAVINGREGIONDATA_H
#define CWCAVINGREGIONDATA_H

#include "cwCaveData.h"

//This is useful for async thread process, extracts all the data from cwCavingRegion
struct cwCavingRegionData {
    QString name;
    QList<cwCaveData> caves;
};


#endif // CWCAVINGREGIONDATA_H
