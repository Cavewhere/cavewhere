#ifndef CWSTATIONPOSITIONMODEL_H
#define CWSTATIONPOSITIONMODEL_H

//Qt includes
#include <QVector3D>
#include <QString>
#include <QMap>

/**
  The station position model holds the position of all the stations
  in a cave.
  */
class cwStationPositionLookup {
public:
    cwStationPositionLookup();

    void clearStations();
    void setPosition(const QString& stationName, const QVector3D& stationPosition);
    QVector3D position(const QString& stationName);
    bool hasPosition(QString stationName) const;

    QMap<QString, QVector3D> positions() const;

private:
    QMap<QString, QVector3D> StationPositions;
};

/**
  Clears all the station of there data
  */
inline void cwStationPositionLookup::clearStations() {
    StationPositions.clear();
}

/**
  Sets the position of the station.  If the station already exists, this will
  overwrite the position of the existing station
  */
inline void cwStationPositionLookup::setPosition(const QString& stationName, const QVector3D& stationPosition) {
    StationPositions[stationName] = stationPosition;
}

/**
  Get's the station position with stationName.  If stationName doesn't exist, this
  will return QVector3D()
  */
inline QVector3D cwStationPositionLookup::position(const QString& stationName) {
    return StationPositions.value(stationName, QVector3D());
}

/**
  Checks if the station position model has the position
  */
inline bool cwStationPositionLookup::hasPosition(QString stationName) const {
    return StationPositions.contains(stationName);
}

/**
  Gets all the positions in the model
  */
inline QMap<QString, QVector3D> cwStationPositionLookup::positions() const {
    return StationPositions;
}



#endif // CWSTATIONPOSITIONMODEL_H
