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

cwStationPositionLookup cwStationPositionLookup::scoped(const QString& prefix) const
{
    if (prefix.isEmpty()) {
        return *this;
    }

    //Keys are canonical, so a canonical prefix matches them exactly.
    const QString key = cwStation::canonicalKey(prefix);
    cwStationPositionLookup lookup;
    for (auto it = StationPositions.lowerBound(key);
         it != StationPositions.constEnd() && it.key().startsWith(key);
         ++it) {
        lookup.StationPositions.insert(it.key().sliced(key.size()), it.value());
    }
    return lookup;
}
