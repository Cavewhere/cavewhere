#ifndef CWCAVEDATA_H
#define CWCAVEDATA_H

//Qt includes
#include <QString>
#include <QUuid>

//Our includes
#include "cwExternalCenterline.h"
#include "cwTripData.h"
#include "cwStationPositionLookup.h"
#include "cwUnits.h"
#include "cwFixStation.h"

struct cwCaveData {
    QString name;
    QList<cwTripData> trips;
    cwStationPositionLookup stationPositionModel; //TODO: remove stationPositionModel?
    QUuid id;
    cwUnits::LengthUnit lengthUnit = cwUnits::Meters;
    cwUnits::LengthUnit depthUnit = cwUnits::Meters;
    QList<cwFixStation> fixStations;
    cwExternalCenterline externalCenterline;
};

#endif // CWCAVEDATA_H
