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

cwCoordinateTransform::DomainCheck domainCheck(const cwFixStation& fix)
{
    // There has to be a coordinate before there is anything to judge. "Mark
    // Station as Fixed" creates the row before the user types anything, and a
    // row whose text can't be read has no coordinate either — both report all
    // three components as zero, and neither is a fix at the origin.
    //
    // Asked of the fix rather than of its numbers. The old test was
    // easting == 0 && northing == 0, and what that decided was up to the CS:
    // 0, 0 in a northern UTM zone inverts to just past the zone's western edge
    // and slips inside the margin, while a southern zone (false northing
    // 10000000) puts it near the pole and a large false origin (EPSG:2154) puts
    // it thousands of km away. It also could not tell an unfilled row from a
    // local grid whose station really is at 0, 0 — which now says so.
    if (fix.state() != cwFixStation::Valid) {
        return {};
    }

    // An absent CS is already excluded above — state() is NoSystem, not Valid.
    return cwCoordinateTransform::domainCheck(fix.inputCS(),
                                              cwGeoPoint(fix.easting(),
                                                         fix.northing(),
                                                         fix.elevation()));
}

bool isDomainValid(const cwFixStation& fix)
{
    const cwCoordinateTransform::DomainCheck check = domainCheck(fix);
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
