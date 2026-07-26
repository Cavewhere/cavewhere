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
