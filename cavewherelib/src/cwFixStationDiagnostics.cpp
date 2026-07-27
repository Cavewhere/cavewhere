/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStationDiagnostics.h"

//Our includes
#include "cwFixStation.h"
#include "cwGeoPoint.h"
#include "cwSurveyNetwork.h"

namespace cwFixStationDiagnostics {

cwCoordinateTransform::DomainCheck domainCheck(const cwFixStation& fix, const QString& globalCS)
{
    // A fix sitting exactly on the origin has no coordinate yet, only the one a
    // default cwFixStation is born with. "Mark Station as Fixed" creates the row
    // before the user types anything, so without this the popup could open on a
    // red warning about a coordinate the user has not entered.
    //
    // Whether it did was up to the CS, which is the worse half: 0, 0 in a
    // northern UTM zone inverts to just past the zone's western edge and slips
    // inside the margin, while a southern zone (false northing 10000000) puts it
    // near the pole and a large false origin (EPSG:2154) puts it thousands of km
    // away. So the same untouched fix warned in some projects and not others.
    // Nothing genuine hides behind the guard: projected eastings don't reach
    // zero (UTM starts at 100000) and 0, 0 geographic is open ocean.
    if (fix.easting() == 0.0 && fix.northing() == 0.0) {
        return {};
    }

    // An absent CS needs no guard here: cwCoordinateTransform::domainCheck defers
    // on an empty one, which is the verdict we want.
    return cwCoordinateTransform::domainCheck(fix.effectiveCS(globalCS),
                                              cwGeoPoint(fix.easting(),
                                                         fix.northing(),
                                                         fix.elevation()));
}

bool isDomainValid(const cwFixStation& fix, const QString& globalCS)
{
    const cwCoordinateTransform::DomainCheck check = domainCheck(fix, globalCS);
    return check.eastingValid && check.northingValid;
}

StationReference classifyStationReference(const QString& stationName,
                                         const cwSurveyNetwork& network)
{
    // Trim before matching, exactly as the survex export does
    // (cwSurvexExporterUtils::validateFixStations) — otherwise a stray trailing
    // space would report a station survex anchors just fine as missing.
    const QString trimmedName = stationName.trimmed();
    if (trimmedName.isEmpty()) {
        return StationReference::Empty;
    }
    // Nothing to check against — a cave whose survey network hasn't been
    // computed yet would flag every named fix, so defer instead.
    if (network.isEmpty()) {
        return StationReference::Ok;
    }
    // CaveWhere station names are case-insensitive (Compass isn't — see
    // CLAUDE.md); hasStation matches that way in one hash probe.
    return network.hasStation(trimmedName)
        ? StationReference::Ok
        : StationReference::Unknown;
}

}
