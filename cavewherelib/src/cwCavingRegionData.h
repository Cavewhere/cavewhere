#ifndef CWCAVINGREGIONDATA_H
#define CWCAVINGREGIONDATA_H

#include "cwCaveData.h"
#include "cwEquate.h"
#include "cwGeoPoint.h"
#include "cwUnits.h"

#include <QStringList>

//This is useful for async thread process, extracts all the data from cwCavingRegion
struct cwCavingRegionData {
    QString name;
    QList<cwCaveData> caves;
    QString globalCoordinateSystem;
    cwGeoPoint worldOrigin;
    cwUnits::UnitSystem unitSystem = cwUnits::Metric;
    QList<cwEquate> equates;
};


#endif // CWCAVINGREGIONDATA_H
