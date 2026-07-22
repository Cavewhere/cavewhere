//Our includes
#include "cwCavernNaming.h"

//Qt includes
#include <QUuid>
#include <QLatin1String>
#include <QLatin1Char>

namespace {
constexpr QLatin1String kCaveNamePrefix("cave_");
constexpr QLatin1String kTripNamePrefix("trip_");
}

QString cwCavernNaming::caveName(const QUuid& caveId)
{
    return kCaveNamePrefix + caveId.toString(QUuid::Id128);
}

QString cwCavernNaming::tripName(const QUuid& tripId)
{
    return kTripNamePrefix + tripId.toString(QUuid::Id128);
}

QString cwCavernNaming::caveScopePrefix(const QUuid& caveId)
{
    return caveName(caveId) + QLatin1Char('.');
}

QString cwCavernNaming::tripScopePrefix(const QUuid& tripId)
{
    return tripName(tripId) + QLatin1Char('.');
}

QString cwCavernNaming::fullScopePrefix(const QUuid& caveId, const QUuid& tripId)
{
    return caveScopePrefix(caveId) + tripScopePrefix(tripId);
}

bool cwCavernNaming::hasTripScope(const QString& caveLocalStation)
{
    return caveLocalStation.startsWith(kTripNamePrefix);
}

QRegularExpression cwCavernNaming::stationRegex()
{
    // Parses cavern's emitted "cave_<32 hex>.<station>" lines back into a
    // (QUuid, station) pair. The tail (capture 2) is deliberately loose:
    //   - native:          cave_<uuid>.<station>
    //   - trip-attached:   cave_<uuid>.trip_<uuid>.<station>
    //   - cave-attached:   cave_<uuid>.<file-begin>.<station>
    // External Survex files can introduce nested *begin scopes that surface as
    // dotted segments inside the tail, and Walls' empty-name quirk can emit
    // trailing spaces; tightening the tail to the native station validator
    // would drop both. The leading \\S guard rejects pure-whitespace tails
    // (e.g. cave_<uuid>. ) so a malformed external file can't pollute the
    // lookup with whitespace keys. The cave UUID prefix is still strictly
    // bounded so the integer-keyed legacy form ("<digit>.station") remains
    // rejected.
    //
    // CaseInsensitiveOption: cwStationPositionLookup keys via
    // cwStation::canonicalKey() which folds station names to lower case
    // (cwStation.h:66). caveName() already emits lowercase via QUuid::Id128 —
    // but the flag keeps the matcher robust if QUuid::Id128 ever changes its
    // case.
    return QRegularExpression(
        QStringLiteral("^cave_([0-9a-fA-F]{32})\\.(\\S.*)$"),
        QRegularExpression::CaseInsensitiveOption);
}
