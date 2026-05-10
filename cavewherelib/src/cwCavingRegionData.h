#ifndef CWCAVINGREGIONDATA_H
#define CWCAVINGREGIONDATA_H

#include "cwCaveData.h"
#include "cwGeoPoint.h"

#include <QStringList>

//This is useful for async thread process, extracts all the data from cwCavingRegion
struct cwCavingRegionData {
    QString name;
    QList<cwCaveData> caves;
    QString globalCS;
    cwGeoPoint worldOrigin;
    QStringList lazLayerSourcePaths; // Persisted paths for region.lazLayers
};


#endif // CWCAVINGREGIONDATA_H
