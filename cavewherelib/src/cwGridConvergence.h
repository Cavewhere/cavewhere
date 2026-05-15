/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGRIDCONVERGENCE_H
#define CWGRIDCONVERGENCE_H

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

//Monad include
#include "Monad/Result.h"

//Qt includes
#include <QString>

/**
 * Computes grid convergence — the angle between true north and grid
 * north for a projected coordinate system at a specific location.
 *
 * Pure function — no QObject, no cave/trip dependency. Resolves
 * (location, sourceCS) → degrees east-positive (positive = grid north
 * lies east of true north). Geographic CRS short-circuits to 0 since
 * there's no grid.
 *
 * Convergence depends on location even within one projection: inside
 * UTM Zone 13N it varies by ~1.7° between the central meridian and
 * the zone edge, so the caller picks a representative point (e.g. a
 * cave's first fix station). No date dependency — unlike magnetic
 * declination, grid convergence is a fixed property of the projection.
 *
 * Magnetic declination is NOT subtracted here. That lives in
 * cwDeclination.
 */
namespace cwGridConvergence {

CAVEWHERE_LIB_EXPORT Monad::Result<double> computeAt(
    const cwGeoPoint& location,
    const QString& sourceCS);

}

#endif // CWGRIDCONVERGENCE_H
