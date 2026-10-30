/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONDIAGNOSTICS_H
#define CWFIXSTATIONDIAGNOSTICS_H

//Qt includes
#include <QHash>
#include <QList>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwCoordinateTransform.h"

class cwFixStation;
class cwSurveyNetwork;
class cwSurveyNode;

/**
 * The rules for judging a single fix station, shared by everything that reports
 * them.
 *
 * A fix can be wrong in ways no solve catches: its coordinate can be impossible
 * for the coordinate system it claims (a transposed digit, the wrong UTM zone),
 * and its station name can match no station in the survey (survex silently drops
 * such a fix). Both questions are asked twice over — cwFixStationDiagnosticsModel
 * flags the offending row inline on FixStationPage, while cwFixStationValidator
 * rolls the same fixes up into a per-cave warning. Sharing the rule is what makes
 * those the *same* verdict rather than two that happen to agree: each turns on
 * judgement calls (how far outside an area of use is too far, when to defer,
 * whether to trim before matching) and a second copy drifts silently — the cell
 * tints with no banner, or the banner names a row that reads clean.
 *
 * Free functions: no QObject, no UI, nothing region-scoped, so the headless
 * validator can ask without reaching through a view model.
 */
namespace cwFixStationDiagnostics {

//! Which of the fix's horizontal coordinates are plausible for the CS it is
//! judged under — its own cwFixStation::inputCS(), never the region's, since a
//! fix that declares no system has no coordinate to judge rather than one to
//! judge under the project's. Split per axis so a caller can tint just the
//! offending cell. Deferral is cwCoordinateTransform::domainCheck's — an absent
//! or un-checkable CS flags nothing — plus one of this layer's own: a fix
//! without a readable coordinate is one nobody has typed one into yet, and
//! flags nothing either.
CAVEWHERE_LIB_EXPORT cwCoordinateTransform::DomainCheck domainCheck(const cwFixStation& fix);

//! domainCheck() collapsed to "the coordinate is plausible" — both axes valid —
//! for callers with no cell to tint.
CAVEWHERE_LIB_EXPORT bool isDomainValid(const cwFixStation& fix);

//! What a solve of a node has to work with, which decides how a fix is judged
//! while the node's network is still empty.
enum class SurveyData {
    Stations, //!< a trip names a station, so a solve is coming
    FromFile, //!< no trip names a station, but a file places the node's stations
    Nothing   //!< no trip names a station and no file places any: the node never gets a solve
};

//! What \a node's solve has to work with. The walk stops at the first named
//! station, so a node with survey data reads only up to its first one.
CAVEWHERE_LIB_EXPORT SurveyData classifySurveyData(const cwSurveyNode& node);

//! How a fix's station name measures up against a cave's survey network.
//! While the network is empty, a named fix defers to Ok until a solve fills it,
//! and is Unknown when \a surveyData says the node has Nothing to solve; an
//! empty name is always flagged, since survex drops such a fix whether or not
//! the survey has been solved.
enum class StationReference {
    Ok,      //!< names an existing station (or a solve is still to come)
    Empty,   //!< no name — an incomplete fix that survex silently drops
    Unknown  //!< a non-empty name that no survey station matches
};

CAVEWHERE_LIB_EXPORT StationReference classifyStationReference(
    const QString& stationName,
    const cwSurveyNetwork& network,
    SurveyData surveyData);

//! Why the export drops a fix whose station name is fine on its own.
enum class StationConflict {
    None,        //!< nothing else fixes the station
    FixedByFile, //!< an attached file fixes the station itself
    Duplicate    //!< an earlier row of the same node fixes the station
};

//! The solved key a fix on \a stationName anchors in \a network: the stored
//! key it resolves to, exactly for a case-pair member and ignoring case
//! otherwise, or its canonicalKey() while \a network has none.
CAVEWHERE_LIB_EXPORT QString fixStationKey(const QString& stationName,
                                           const cwSurveyNetwork& network);

//! How row \a row of \a fixes conflicts with the other fixes on its station.
//! \a fileFixedStations is cwSurveyNode::fileFixedStations(), and each fix
//! names the station fixStationKey() resolves it to in \a network, so fixes
//! on P2 and p2 of a case pair are two fixes. Follows the export: a file's own
//! fix wins over every row, and of two rows the first wins.
CAVEWHERE_LIB_EXPORT StationConflict classifyStationConflict(
    const QList<cwFixStation>& fixes, qsizetype row,
    const QHash<QString, QString>& fileFixedStations, const cwSurveyNetwork& network);

//! Why a fix on \a station is dropped when the attached file \a fileName fixes
//! that station itself. Shared by the export's error and the Fix Stations
//! page, so both say the same thing.
CAVEWHERE_LIB_EXPORT QString fileFixCollisionMessage(const QString& fileName,
                                                     const QString& station);

}

#endif // CWFIXSTATIONDIAGNOSTICS_H
