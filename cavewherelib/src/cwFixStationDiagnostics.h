/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONDIAGNOSTICS_H
#define CWFIXSTATIONDIAGNOSTICS_H

//Qt includes
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwCoordinateTransform.h"

class cwFixStation;
class cwSurveyNetwork;

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
//! judged under (cwFixStation::effectiveCS with `globalCS` as the fallback), so a
//! caller can tint just the offending cell. Deferral is
//! cwCoordinateTransform::domainCheck's — an absent or un-checkable CS flags
//! nothing.
CAVEWHERE_LIB_EXPORT cwCoordinateTransform::DomainCheck domainCheck(const cwFixStation& fix,
                                                                   const QString& globalCS);

//! domainCheck() collapsed to "the coordinate is plausible" — both axes valid —
//! for callers with no cell to tint.
CAVEWHERE_LIB_EXPORT bool isDomainValid(const cwFixStation& fix, const QString& globalCS);

//! How a fix's station name measures up against a cave's survey network.
//! A named fix defers to Ok while the network is empty (nothing to check
//! against yet); an empty name is always flagged, since survex drops such a fix
//! whether or not the survey has been solved.
enum class StationReference {
    Ok,      //!< names an existing station (or nothing to check against)
    Empty,   //!< no name — an incomplete fix that survex silently drops
    Unknown  //!< a non-empty name that no survey station matches
};

CAVEWHERE_LIB_EXPORT StationReference classifyStationReference(const QString& stationName,
                                                              const cwSurveyNetwork& network);

}

#endif // CWFIXSTATIONDIAGNOSTICS_H
