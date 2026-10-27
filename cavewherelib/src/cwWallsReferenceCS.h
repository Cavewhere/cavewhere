/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWWALLSREFERENCECS_H
#define CWWALLSREFERENCECS_H

//Our includes
#include "cwGlobals.h"

//Dewalls includes
#include "wallsprojectparser.h"

//Qt includes
#include <QString>

/**
 * The coordinate systems a Walls project's `.REF` names, as PROJ reads them:
 * \a rect for `#FIX` lines given as grid coordinates (which need the datum and
 * the zone), \a geo for the ones given as latitude and longitude (which need
 * only the datum). Both are empty when the `.REF` said nothing usable — a bare
 * .srv imported without its project file has none — and \a unmappedDatum then
 * carries the datum name the file did give, so the warning can name it.
 */
struct WallsReferenceCS {
    QString rect;
    QString geo;
    QString unmappedDatum;
};

//! What \a reference says the project's `#FIX` coordinates are in, as cavern
//! reads them (survex datain.c): img's UTM system for the datum and zone, and
//! img's geographic system for the datum.
CAVEWHERE_LIB_EXPORT WallsReferenceCS wallsReferenceCS(const dewalls::GeoReferencePtr& reference);

#endif // CWWALLSREFERENCECS_H
