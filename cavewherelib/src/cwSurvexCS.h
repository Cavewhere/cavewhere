/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXCS_H
#define CWSURVEXCS_H

//Qt includes
#include <QString>

//Std includes
#include <optional>

class QTextStream;

/**
 * How CaveWhere writes a coordinate system for survex, and how it reads one
 * back. Both directions live here so the two stay one encoding: what
 * toSurvexCS() quotes behind survex's CUSTOM keyword, fromSurvexCS() unquotes.
 */
namespace cwSurvexCS {

//! What a survex `*cs` argument names, said in PROJ's vocabulary.
struct Parsed {
    //! Empty when the spelling names no CRS: LOCAL, or one we can't map.
    QString projCS;

    //! LAT-LONG: the `*fix` pair leads with latitude, so its first two numbers
    //! transpose on the way to cwFixStation, which always takes easting
    //! (longitude for a geographic system) first.
    bool latitudeFirst = false;
};

/**
 * \a csArgument — the text following `*cs` — as PROJ reads it, or nothing when
 * survex doesn't define the spelling. LOCAL is defined and still yields an
 * empty projCS — it says "deliberately ungeoreferenced" — so absence is what
 * tells a warning apart from an ordinary local grid.
 *
 * The mapping is cavern's own: each spelling yields the PROJ string cavern
 * builds from it (survex/src/commands.c, cmd_cs). The one departure is
 * LAT-LONG, which cavern refuses outright; CaveWhere reads it as EPSG:4326
 * with the coordinate pair transposed, which is what the spelling means.
 */
std::optional<Parsed> fromSurvexCS(const QString& csArgument);

//! \a cs rendered as survex *cs syntax. Survex names a system by keyword or
//! authority code; anything else — a raw PROJ string, which the project's own
//! local projection is — has to be quoted behind the CUSTOM keyword, or cavern
//! refuses it with "Unknown coordinate system" (survex/src/commands.c, the
//! CS_CUSTOM branch). The test is what a bare survex name may contain rather
//! than what a PROJ string looks like, so WKT is quoted too.
QString toSurvexCS(const QString& cs);

//! Emit a `*cs` (or `*cs out`) line for \a cs, quoted as survex needs. Every
//! emitter goes through here so a new one can't drop the CUSTOM quoting and
//! fail the solve.
void writeCsLine(QTextStream& stream, const QString& cs, bool isOutput = false);

} // namespace cwSurvexCS

#endif // CWSURVEXCS_H
