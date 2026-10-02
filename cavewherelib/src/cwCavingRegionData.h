#ifndef CWCAVINGREGIONDATA_H
#define CWCAVINGREGIONDATA_H

#include "cwCaveData.h"
#include "cwEquate.h"
#include "cwGeoReference.h"
#include "cwUnits.h"

#include <QStringList>

//! The local projection and its lifecycle, as stored — see cwGeoReference.
struct cwGeoReferenceData {
    cwGeoReference::State state = cwGeoReference::Ungeoreferenced;
    QString localCoordinateSystem;
    cwGeoReference::Anchor anchor;
    QString verticalDatum;
};

//This is useful for async thread process, extracts all the data from cwCavingRegion
struct cwCavingRegionData {
    QString name;
    QList<cwCaveData> caves;
    cwUnits::UnitSystem unitSystem = cwUnits::Metric;
    cwGeoReferenceData geoReference;
    QList<cwEquate> equates;
};


#endif // CWCAVINGREGIONDATA_H
