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
#include <QString>
#include <QMap>
#include <QHash>
#include <QList>

//Our includes
#include "cwGlobals.h"
#include "cwStation.h"

/**
  Where a cave's splays end, keyed by the cwStation::canonicalKey of the station
  each splay hangs off — the same key space cwStationPositionLookup uses.

  A splay ends at an anonymous station, so its tip has no name to key on and no
  place in the position lookup; the station it hangs off carries the whole list.
  */
using cwSplayTipsByStation = QHash<QString, QList<QVector3D>>;

/**
  The station position model holds the position of all the stations
  in a cave.
  */
class CAVEWHERE_LIB_EXPORT cwStationPositionLookup {
public:
    cwStationPositionLookup();

    void clearStations();
    void setPosition(const QString& stationName, const QVector3D& stationPosition);

    //! Stores \a position under \a key exactly as given, without folding case.
    void setPositionAtKey(const QString& key, const QVector3D& position);

    //! The stored key \a stationName resolves to, or empty when none does.
    QString key(const QString& stationName) const;

    QVector3D position(const QString& stationName) const;
    bool hasPosition(QString stationName) const;
    bool isEmpty() const { return StationPositions.isEmpty(); }

    QMap<QString, QVector3D> positions() const;

    //! The stations whose names start with \a prefix (case-insensitively),
    //! keyed by the rest of the name. An empty prefix answers this lookup.
    cwStationPositionLookup scoped(const QString& prefix) const;

    bool operator==(const cwStationPositionLookup& other) const = default;

private:
    QMap<QString, QVector3D> StationPositions;

    QMap<QString, QVector3D>::const_iterator find(const QString& stationName) const;
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
    StationPositions[cwStation::canonicalKey(stationName)] = stationPosition;
}

/**
  Gets all the positions in the model
  */
inline QMap<QString, QVector3D> cwStationPositionLookup::positions() const {
    return StationPositions;
}



#endif // CWSTATIONPOSITIONMODEL_H
