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

void cwStationPositionLookup::setPositionAtKey(const QString& key, const QVector3D& position)
{
    StationPositions[key] = position;
}

QMap<QString, QVector3D>::const_iterator cwStationPositionLookup::find(const QString& stationName) const
{
    const auto exact = StationPositions.constFind(stationName);
    if (exact != StationPositions.constEnd()) {
        return exact;
    }
    const QString canonical = cwStation::canonicalKey(stationName);
    if (canonical == stationName) {
        return StationPositions.constEnd();
    }
    return StationPositions.constFind(canonical);
}

QString cwStationPositionLookup::key(const QString& stationName) const
{
    const auto it = find(stationName);
    return it != StationPositions.constEnd() ? it.key() : QString();
}

/**
  The position stationName resolves to, or QVector3D() when none does
  */
QVector3D cwStationPositionLookup::position(const QString& stationName) const
{
    const auto it = find(stationName);
    return it != StationPositions.constEnd() ? it.value() : QVector3D();
}

bool cwStationPositionLookup::hasPosition(QString stationName) const
{
    return find(stationName) != StationPositions.constEnd();
}

cwStationPositionLookup cwStationPositionLookup::scoped(const QString& prefix) const
{
    if (prefix.isEmpty()) {
        return *this;
    }

    //Keys keep their survey levels lowercase, so a lowercase prefix matches them exactly.
    const QString key = cwStation::canonicalKey(prefix);
    cwStationPositionLookup lookup;
    for (auto it = StationPositions.lowerBound(key);
         it != StationPositions.constEnd() && it.key().startsWith(key);
         ++it) {
        lookup.setPositionAtKey(it.key().sliced(key.size()), it.value());
    }
    return lookup;
}
