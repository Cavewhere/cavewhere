/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "FixStationFixtureHelper.h"

#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStationModel.h"

cwFixStation makeFix(const QString& name,
                     const QString& cs,
                     double easting,
                     double northing,
                     double elevation)
{
    cwFixStation fix;
    fix.setStationName(name);
    fix.setInputCS(cs);
    fix.setCoordinate(easting, northing, elevation);
    return fix;
}

cwCave* addCaveWithFixes(cwCavingRegion* region, const QList<cwFixStation>& fixes)
{
    auto* cave = new cwCave();
    region->addCave(cave);
    cave->fixStations()->setFixStations(fixes);
    return cave;
}
