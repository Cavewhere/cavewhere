#ifndef CWCAVERNNAMING_H
#define CWCAVERNNAMING_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QString>
#include <QRegularExpression>

class QUuid;

/**
 * One home for the synthetic-name grammar the line-plot driver uses to carry a
 * cave/trip identity through the worker's intermediate .svx and back out of
 * cavern's emitted station lines.
 *
 * The driver rewrites each cave to "cave_<32 hex of QUuid::Id128>" and wraps an
 * externally-attached trip's *include in "trip_<32 hex>", so cavern echoes a
 * fully-qualified station as cave_<hex>.trip_<hex>.<tail> (external) or
 * cave_<hex>.<tail> (native). stationRegex() decodes the cave half back into a
 * (QUuid, station) pair. The names are never user-visible: the friendly-name
 * exporter operates on the original snapshot before this rewrite.
 *
 * Every producer (the survex exporter, the worker encoder) and every consumer
 * (splitLookupByCave, the cave-network mirror, the cwTrip solved-station
 * accessors, the geometry/label enumeration, cwScopeStationListModel) shares
 * these functions so the encode and decode sides can never drift.
 */
namespace cwCavernNaming {

//! "cave_<32 hex of QUuid::Id128>"
CAVEWHERE_LIB_EXPORT QString caveName(const QUuid& caveId);

//! "trip_<32 hex of QUuid::Id128>"
CAVEWHERE_LIB_EXPORT QString tripName(const QUuid& tripId);

//! "cave_<hex>." — the cave-scope prefix a cave-local key carries in the region network.
CAVEWHERE_LIB_EXPORT QString caveScopePrefix(const QUuid& caveId);

//! "trip_<hex>." — the trip-scope prefix an externally-attached trip's cave-local keys carry.
CAVEWHERE_LIB_EXPORT QString tripScopePrefix(const QUuid& tripId);

//! "cave_<hex>.trip_<hex>." — the full region-network prefix for an external trip's stations.
CAVEWHERE_LIB_EXPORT QString fullScopePrefix(const QUuid& caveId, const QUuid& tripId);

//! True when a cave-local station name still carries a trip scope (i.e. it came
//! from an externally-attached trip rather than a native chunk).
CAVEWHERE_LIB_EXPORT bool hasTripScope(const QString& caveLocalStation);

//! Decodes a cavern-emitted "cave_<32 hex>.<station>" line into its (QUuid,
//! station) parts (match capture 1 = hex, capture 2 = station tail).
CAVEWHERE_LIB_EXPORT QRegularExpression stationRegex();

}

#endif // CWCAVERNNAMING_H
