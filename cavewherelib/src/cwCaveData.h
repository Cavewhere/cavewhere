#ifndef CWCAVEDATA_H
#define CWCAVEDATA_H

//Qt includes
#include <QString>

//Our includes
#include "cwTripData.h"
#include "cwStationPositionLookup.h"

struct cwCaveData {
    QString name;
    QList<cwTripData> trips;
    cwStationPositionLookup stationPositionModel;
};

#endif // CWCAVEDATA_H
