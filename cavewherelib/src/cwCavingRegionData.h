#ifndef CWCAVINGREGIONDATA_H
#define CWCAVINGREGIONDATA_H

#include "cwCaveData.h"
#include "cwGeoPoint.h"

#include <QStringList>
#include <QUuid>

//This is useful for async thread process, extracts all the data from cwCavingRegion
struct cwCavingRegionData {
    QString name;
    QList<cwCaveData> caves;
    QString globalCoordinateSystem;
    cwGeoPoint worldOrigin;
    QUuid defaultPaletteId;
};


#endif // CWCAVINGREGIONDATA_H
