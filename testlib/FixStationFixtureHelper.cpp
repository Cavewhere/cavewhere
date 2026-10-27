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
#include "../testcases/TestHelper.h"

#include <QDebug>

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
    // The frame follows the fixes in a queued pass. REQUIRE is unavailable in
    // testlib, so a stalled pass surfaces as the caller's next assertion.
    if (!waitForQueuedEvents()) {
        qWarning() << "addCaveWithFixes: the queued frame pass timed out";
    }
    return cave;
}
