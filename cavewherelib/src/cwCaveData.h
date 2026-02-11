#ifndef CWCAVEDATA_H
#define CWCAVEDATA_H

//Qt includes
#include <QString>
#include <QUuid>

//Our includes
#include "cwTripData.h"
#include "cwStationPositionLookup.h"

struct cwCaveData {
    QString name;
    QList<cwTripData> trips;
    cwStationPositionLookup stationPositionModel;
    QUuid id;
};

#endif // CWCAVEDATA_H
