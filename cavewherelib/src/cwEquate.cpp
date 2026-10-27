/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwEquate.h"

//Qt includes
#include <QSet>

bool cwEquate::isValid() const
{
    if (m_stations.size() < 2) {
        return false;
    }

    QSet<cwStationHandle> unique;
    for (const cwStationHandle& station : m_stations) {
        if (!station.isValid()) {
            return false;
        }
        unique.insert(station);
    }

    //At least two distinct endpoints - a station equated only to itself ties
    //nothing.
    return unique.size() >= 2;
}
