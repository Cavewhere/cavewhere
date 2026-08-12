/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXCS_H
#define CWSURVEXCS_H

//Qt includes
#include <QHash>
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

/**
 * Where a `*cs` line may put a `.prj` sidecar, and which ones the file it is
 * writing already has.
 *
 * WKT has no inline spelling: survex reads a quoted argument with read_string(),
 * which has no escape syntax, so the first `"` in the payload ends the string
 * and cavern takes `PROJCRS[` for the whole system. It reads the system from its
 * own file instead — `*cs out CUSTOM @<stem>.prj`, resolved relative to the
 * `.svx` (survex/src/commands.c, read_cs_from_file), which is why a bare file
 * name is enough. Survex 1.4.23 and newer.
 *
 * One `.svx` can need more than one sidecar: its `*cs out` and each fix row's
 * own `*cs` are separate strings, and any of them can be WKT. So stems are
 * handed out per unique system — `region.prj`, `region-2.prj`, and so on — and a
 * system asked for twice gets back the file it already has. One stem for the
 * whole file would let the last writer decide what every reference resolves to,
 * which georeferences the cave somewhere else without saying so.
 *
 * Nothing reaches disk until write(). Call that once the `.svx` itself is
 * safely written: on the rule path the `.svx` goes through QSaveFile and appears
 * only at the end, and a sidecar beside a file that never appeared describes a
 * system for nobody.
 */
class SidecarWriter
{
public:
    SidecarWriter() = default;
    explicit SidecarWriter(const QString& survexPath);

    //! The `.svx` the sidecars belong beside. They take its stem and its
    //! directory, and any name reserved for the previous one is forgotten.
    void setSurvexFile(const QString& survexPath);

    //! The sidecar file name to reference for \a cs, taking one when this is the
    //! first request for that system. Empty when no `.svx` says where to put it,
    //! which leaves the caller its old inline spelling.
    QString reserve(const QString& cs);

    //! Write every reserved sidecar. Empty when they all got there, and
    //! otherwise what went wrong with the first one that didn't — a sidecar that
    //! fails to write fails the solve, since cavern refuses a `*cs` whose file it
    //! can't open, rather than quietly solving somewhere else.
    QString write() const;

private:
    QString m_directory;
    QString m_stem;
    QHash<QString, QString> m_fileNameByCS;
};

//! \a cs rendered as survex *cs syntax. Survex names a system by keyword or
//! authority code; anything else — a raw PROJ string, which the project's own
//! local projection is — has to be quoted behind the CUSTOM keyword, or cavern
//! refuses it with "Unknown coordinate system" (survex/src/commands.c, the
//! CS_CUSTOM branch). The test is what a bare survex name may contain rather
//! than what a PROJ string looks like. A system carrying a `"` of its own — WKT
//! — has no inline spelling at all and goes to \a sidecars for a file.
QString toSurvexCS(const QString& cs, SidecarWriter& sidecars);

//! Emit a `*cs` (or `*cs out`) line for \a cs, spelled as survex needs. Every
//! emitter goes through here so a new one can't drop the CUSTOM quoting and
//! fail the solve.
void writeCsLine(QTextStream& stream, SidecarWriter& sidecars, const QString& cs,
                 bool isOutput = false);

} // namespace cwSurvexCS

#endif // CWSURVEXCS_H
