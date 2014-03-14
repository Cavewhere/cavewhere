/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwStationPositionLookup.h"

cwStationPositionLookup::cwStationPositionLookup()
{
}

/**
 * @brief cwStationPositionLookup::setPosition
 * @param stationName
 * @param stationPosition
 */
void cwStationPositionLookup::setPosition(const QString &stationName, const QVector3D &stationPosition)
{
    if(hasStation(stationName)) {
        Stations[stationName].setPosition(stationPosition);
    } else {
        cwStationPosition newPosition;
        newPosition.setPosition(stationPosition);
        insertStation(stationName, newPosition);
    }
}
