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
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNetwork.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

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
    return cwCoordinateTransform::domainCheck(fix.inputCS(), fix.position());
}

bool isDomainValid(const cwFixStation& fix)
{
    return domainCheck(fix).isValid();
}

StationReference classifyStationReference(const QString& stationName,
                                         const cwSurveyNetwork& network,
                                         SurveyData surveyData)
{
    // Trim before matching, exactly as the survex export does
    // (cwSurvexExporterUtils::validateFixStations) — otherwise a stray trailing
    // space would report a station survex anchors just fine as missing.
    const QString trimmedName = stationName.trimmed();
    if (trimmedName.isEmpty()) {
        return StationReference::Empty;
    }
    // An empty network is a solve still to come, unless the node has nothing
    // to solve: then it stays empty, and no station can match.
    if (network.isEmpty()) {
        return surveyData == SurveyData::Nothing
            ? StationReference::Unknown
            : StationReference::Ok;
    }
    // CaveWhere station names are case-insensitive (Compass isn't — see
    // CLAUDE.md); hasStation matches that way in one hash probe.
    return network.hasStation(trimmedName)
        ? StationReference::Ok
        : StationReference::Unknown;
}

namespace {

bool hasNamedStation(const cwTrip& trip)
{
    for (const cwSurveyChunk* chunk : trip.chunks()) {
        for (const cwStation& station : chunk->stations()) {
            if (!station.name().trimmed().isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

//! Walks \a node's trips, then its children, and stops at the first named
//! station. \a placedByFile collects whether anything on the way is backed by
//! a file, which only matters when no station turns up.
bool walkForStations(const cwSurveyNode& node, bool& placedByFile)
{
    for (const cwTrip* trip : node.trips()) {
        if (hasNamedStation(*trip)) {
            return true;
        }
        placedByFile = placedByFile || trip->externallyBacked();
    }
    for (const cwSurveyNode* child : node.childNodes()) {
        placedByFile = placedByFile || child->isSourced() || child->externallyBacked();
        if (walkForStations(*child, placedByFile)) {
            return true;
        }
    }
    return false;
}

} // namespace

SurveyData classifySurveyData(const cwSurveyNode& node)
{
    bool placedByFile = node.sourceRoot() != nullptr || node.externallyBacked();
    if (walkForStations(node, placedByFile)) {
        return SurveyData::Stations;
    }
    return placedByFile ? SurveyData::FromFile : SurveyData::Nothing;
}

QString fixStationKey(const QString& stationName, const cwSurveyNetwork& network)
{
    const QString trimmed = stationName.trimmed();
    const QString solved = network.key(trimmed);
    return solved.isEmpty() ? cwStation::canonicalKey(trimmed) : solved;
}

StationConflict classifyStationConflict(const QList<cwFixStation>& fixes, qsizetype row,
                                        const QHash<QString, QString>& fileFixedStations,
                                        const cwSurveyNetwork& network)
{
    const QString key = fixStationKey(fixes.at(row).stationName(), network);
    if (key.isEmpty()) {
        return StationConflict::None;
    }
    if (fileFixedStations.contains(key)) {
        return StationConflict::FixedByFile;
    }
    for (qsizetype earlier = 0; earlier < row; ++earlier) {
        if (fixStationKey(fixes.at(earlier).stationName(), network) == key) {
            return StationConflict::Duplicate;
        }
    }
    return StationConflict::None;
}

QString fileFixCollisionMessage(const QString& fileName, const QString& station)
{
    return QStringLiteral("%1 already fixes %2; remove that fix from the file or fix another "
                          "station")
        .arg(fileName, station);
}

}
