/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef BOULDERFIXTUREHELPER_H
#define BOULDERFIXTUREHELPER_H

#include <QDateTime>
#include <QString>

#include <memory>

#include "CaveWhereTestLibExport.h"

class cwCave;
class cwCavingRegion;
class cwTrip;
class cwTripCalibration;

extern const QString CAVEWHERE_TESTLIB_EXPORT kWgs84;

/**
 * Build a QDateTime at midnight UTC. The IGRF auto-declination path is
 * sensitive to date drift across decades, so tests pin an exact instant
 * rather than relying on the local time zone.
 */
QDateTime CAVEWHERE_TESTLIB_EXPORT makeUtcDate(int year, int month, int day);

/**
 * Owning fixture for declination tests. The region holds the cave (and
 * transitively trip + calibration) via QObject parenting, so the
 * unique_ptr handles cleanup when a REQUIRE early-exits.
 */
struct CAVEWHERE_TESTLIB_EXPORT BoulderFixture {
    std::unique_ptr<cwCavingRegion> region;
    cwCave* cave;
    cwTrip* trip;
    cwTripCalibration* calibration;
};

/**
 * Region with one cave, one trip dated 2024-06-01, and one WGS84 fix
 * station at Boulder, CO. No chunks. Used by the declination resolution
 * and warning tests.
 */
BoulderFixture CAVEWHERE_TESTLIB_EXPORT buildBoulderFixture();

#endif // BOULDERFIXTUREHELPER_H
