#ifndef CWCAVINGREGIONDATA_H
#define CWCAVINGREGIONDATA_H

#include "cwCaveData.h"
#include "cwGeoPoint.h"
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
    QString globalCoordinateSystem;
    cwGeoPoint worldOrigin;
    cwUnits::UnitSystem unitSystem = cwUnits::Metric;
    cwGeoReferenceData geoReference;
};


#endif // CWCAVINGREGIONDATA_H
