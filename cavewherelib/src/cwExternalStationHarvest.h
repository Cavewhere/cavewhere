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

} // namespace cwExternalStationHarvest

#endif // CWEXTERNALSTATIONHARVEST_H
