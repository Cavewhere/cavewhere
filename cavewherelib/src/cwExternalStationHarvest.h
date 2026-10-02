/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALSTATIONHARVEST_H
#define CWEXTERNALSTATIONHARVEST_H

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"

#include <QString>
#include <QStringList>

/**
 * \brief Reads the station names out of an external centerline file by solving
 * it on its own.
 *
 * The region solve is not a reliable source of an attachment's station names.
 * Cavern drops a survey that nothing fixes and nothing ties in (netartic.c
 * warning 45, "Survey not all connected to fixed stations"), so the names of
 * exactly the attachments the user needs to tie in never reach the .3d.
 *
 * Solving the file alone dodges that: a driver of just "*include <entry>" has
 * no fixed point and no coordinate system, which is the case netskel.c handles
 * by fixing the first station at the origin (message 72) and solving anyway.
 * The positions that come back are in an arbitrary frame and are discarded —
 * only the names are kept.
 */
namespace cwExternalStationHarvest {

/**
 * Returns the station names \a entryFile declares, canonicalized with
 * cwStation::canonicalKey and sorted.
 *
 * \warning Only the stations reachable from the one station netskel fixes.
 * A file holding two surveys with no leg and no "*equate" between them has
 * two components, netskel's implicit fix lands in one of them, and articulate()
 * drops the other (warning 45) — so its names come back missing, with no error.
 * Fixing this means fixing one station per connected component rather than one
 * per file, which is a change to the vendored cavern — issue #651.
 *
 * The names are in the attachment's own namespace — the file's own "*begin"
 * blocks are the only naming levels, since the harvest driver adds none. That
 * is the same spelling cwTrip::solvedStations() yields for the trip once the
 * region solve does place it: the main driver wraps the identical "*include"
 * in the cave's and trip's "*begin" blocks, and cwTrip::solvedStations()
 * strips exactly that wrapper back off.
 *
 * A successful result always names at least one station, and empty names always
 * come with an error — a caller cannot otherwise tell "this file declares
 * nothing" from "reading it failed".
 *
 * On a file cavern cannot read, the error carries cavern's own log text, minus
 * the lines naming the throwaway driver — the per-file verdict the region solve
 * can only give for the region as a whole.
 * \a entryFile may name any format cavern reads through "*include": Survex,
 * Compass (.dat/.mak) and Walls (.srv/.wpj).
 *
 * Runs cavern in-process, so it is serialized against every other cavern
 * caller and must not be called on the main thread.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<QStringList> harvest(const QString& entryFile);

//! What harvestWithFixes() reads from one file.
struct Stations {
    //! The same names harvest() returns.
    QStringList names;
    //! The subset of names the file fixes itself, canonical and in .3d order.
    //! The origin cavern invents for a file with no fix is excluded.
    QStringList fixedNames;
};

/**
 * harvest(), plus the stations \a entryFile fixes itself. A driver fix on one
 * of those collides with the file's own: cavern raises error 46 when the two
 * coordinates differ, whichever comes first.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<Stations> harvestWithFixes(const QString& entryFile);

/**
 * Returns the station names of every component of \a entryFile that holds one
 * of \a seedStations, canonicalized and sorted like harvest().
 *
 * Each seed is fixed at the origin, so cavern places the seed's component
 * whether or not anything else ties it in. Seeds are named in the file's own
 * namespace. With a seed in every component it reaches, this reads the names
 * of the surveys a region solve dropped as hanging, which harvest() alone
 * cannot.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<QStringList> harvestComponents(const QString& entryFile,
                                                                  const QStringList& seedStations);

} // namespace cwExternalStationHarvest

#endif // CWEXTERNALSTATIONHARVEST_H
