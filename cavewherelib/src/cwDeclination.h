/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDECLINATION_H
#define CWDECLINATION_H

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

//Monad include
#include "Monad/Result.h"

//Qt includes
#include <QDateTime>
#include <QString>

/**
 * Computes magnetic declination via the IGRF model from survex.
 *
 * Pure function — no QObject, no cave/trip dependency. Resolves
 * (location, sourceCS, date) → degrees east-positive. Caller is
 * responsible for choosing a representative location (e.g. a cave's
 * first fix station) and threading a sensible date (e.g. the trip's
 * survey date).
 *
 * Grid convergence is NOT subtracted here. That's a property of the
 * projection alone (no date dependency) and lives in cwGridConvergence.
 */
namespace cwDeclination {

CAVEWHERE_LIB_EXPORT Monad::Result<double> compute(
    const cwGeoPoint& location,
    const QString& sourceCS,
    const QDateTime& date);

}

#endif // CWDECLINATION_H
