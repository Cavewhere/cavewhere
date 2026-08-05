/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef FIXSTATIONFIXTUREHELPER_H
#define FIXSTATIONFIXTUREHELPER_H

#include "cwFixStation.h"

#include <QList>
#include <QString>

#include "CaveWhereTestLibExport.h"

class cwCave;
class cwCavingRegion;

/**
 * A fix station named \a name, sitting at (\a easting, \a northing,
 * \a elevation) under \a cs. Written as one coordinate, so the components
 * survive: setting them one at a time loses the earlier two.
 */
cwFixStation CAVEWHERE_TESTLIB_EXPORT makeFix(const QString& name,
                                              const QString& cs,
                                              double easting,
                                              double northing,
                                              double elevation);

/**
 * A new cave in \a region holding \a fixes. The first cave to carry a
 * georeferenced fix anchors the project's local projection, so a test that
 * wants a cave with a non-zero grid convergence adds the anchor cave first
 * and offsets the cave under test from it.
 */
cwCave* CAVEWHERE_TESTLIB_EXPORT addCaveWithFixes(cwCavingRegion* region,
                                                  const QList<cwFixStation>& fixes);

#endif // FIXSTATIONFIXTUREHELPER_H
