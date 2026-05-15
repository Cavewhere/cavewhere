/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "BoulderFixtureHelper.h"

#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

#include <QTimeZone>

const QString kWgs84 = QStringLiteral("EPSG:4326");

QDateTime makeUtcDate(int year, int month, int day)
{
    return QDateTime(QDate(year, month, day), QTime(0, 0), QTimeZone::UTC);
}

BoulderFixture buildBoulderFixture()
{
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    region->addCave(cave);

    cwFixStation fix;
    fix.setStationName(QStringLiteral("Entrance"));
    fix.setInputCS(kWgs84);
    fix.setEasting(-105.27);
    fix.setNorthing(40.015);
    fix.setElevation(1655.0);
    cave->fixStations()->appendFixStation(fix);

    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setDate(makeUtcDate(2024, 6, 1));

    return { std::move(region), cave, trip, trip->calibrations() };
}
