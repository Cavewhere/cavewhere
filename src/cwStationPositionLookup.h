/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTATIONPOSITIONMODEL_H
#define CWSTATIONPOSITIONMODEL_H

//Qt includes
#include <QVector3D>
#include <QMatrix3x3>
#include <QString>
#include <QMap>

//Our includes
#include "cwStationPosition.h"

/**
  The station position model holds the position of all the stations
  in a cave.
  */
class cwStationPositionLookup {
public:
    cwStationPositionLookup();

    void clearStations();

    void setPosition(const QString& stationName, const QVector3D& stationPosition);
    QVector3D position(const QString& stationName) const;

    QMatrix3x3 covariance(const QString& stationName) const;

    bool hasStation(QString stationName) const;

    QMap<QString, cwStationPosition> positions() const;

    void insertStation(const QString& stationName, cwStationPosition station);
    cwStationPosition station(const QString& stationName);

private:
    QMap<QString, cwStationPosition> Stations;
//    QMap<QString, QVector3D> StationPositions;
//    QMap<QString, QMatrix3x3> StationConvariance;
};

/**
  Clears all the station of there data
  */
inline void cwStationPositionLookup::clearStations() {
    Stations.clear();
}



///**
//  Sets the position of the station.  If the station already exists, this will
//  overwrite the position of the existing station
//  */
//inline void cwStationPositionLookup::setPosition(const QString& stationName, const QVector3D& stationPosition) {
//    StationPositions[stationName.toLower()] = stationPosition;
//}

/**
  Get's the station position with stationName.  If stationName doesn't exist, this
  will return QVector3D()
  */
inline QVector3D cwStationPositionLookup::position(const QString& stationName) const {
    return Stations.value(stationName.toLower()).position();
}

inline QMatrix3x3 cwStationPositionLookup::covariance(const QString &stationName) const
{
    return Stations.value(stationName.toLower()).convariance();
}

///**
// * @brief cwStationPositionLookup::setConvariance
// * @param stationName
// * @param matrix
// */
//inline void cwStationPositionLookup::setConvariance(const QString &stationName, const QMatrix3x3 &matrix)
//{
//    StationConvariance[stationName.toLower()] = matrix;
//}

///**
// * @brief cwStationPositionLookup::convariance
// * @param stationName
// * @return
// */
//inline QMatrix3x3 cwStationPositionLookup::convariance(const QString& stationName) const
//{
//    return StationConvariance.value(stationName.toLower());
//}

/**
  Checks if the station position model has the position
  */
inline bool cwStationPositionLookup::hasStation(QString stationName) const {
    return Stations.contains(stationName.toLower());
}

/**
  Gets all the positions in the model
  */
inline QMap<QString, cwStationPosition> cwStationPositionLookup::positions() const {
    return Stations;
}

inline void cwStationPositionLookup::insertStation(const QString &stationName, cwStationPosition station)
{
    Stations.insert(stationName, station);
}



#endif // CWSTATIONPOSITIONMODEL_H
