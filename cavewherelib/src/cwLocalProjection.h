/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLOCALPROJECTION_H
#define CWLOCALPROJECTION_H

//Qt includes
#include <QString>

//Our includes
#include "cwGeoPoint.h"
#include "cwGlobals.h"

/**
 * Derives the project's working frame: a low-distortion projection (LDP), a
 * transverse Mercator centered on the project itself.
 *
 * The recipe is fixed — <tt>+proj=tmerc +k_0=1 +x_0=0 +y_0=0</tt> at the
 * anchor's latitude and longitude, on the anchor's own datum:
 *
 *   - convergence is zero on the central meridian, which runs through the
 *     origin, and stays under half a degree out to ~45 km;
 *   - scale error is ~1 ppm at 10 km and ~11 ppm at 30 km, against UTM's
 *     designed worst case of ~400 ppm;
 *   - coordinates stay small enough to be float-safe on the GPU without an
 *     origin-offset hack;
 *   - it is analytic and invertible from the string alone, so any point
 *     round-trips to latitude/longitude exactly.
 *
 * <b>The result is stored, never recomputed.</b> Two machines with different
 * PROJ data must not disagree about where the project is, and re-deriving at
 * load would make the frame a live function of the data rather than a value
 * that changes only on the events that are allowed to move it. The lifecycle
 * that decides when a new string is derived lives on cwGeoReference.
 *
 * The datum is pinned from the anchor's coordinate system rather than chosen:
 * a compound CRS (what lidar declares) contributes its horizontal half only —
 * the vertical is recorded elsewhere and never applied, because converting
 * heights between the geoid and the ellipsoid would shift the whole project by
 * the geoid separation.
 */
class CAVEWHERE_LIB_EXPORT cwLocalProjection
{
public:
    /**
     * The LDP centered on (\a latitude, \a longitude) — degrees, on the datum
     * of \a datumSourceCS. An empty or unparseable \a datumSourceCS falls back
     * to WGS84, which is what a typed coordinate means when nothing says
     * otherwise.
     *
     * Returns "" for a latitude/longitude that isn't a location, or when PROJ
     * can't express the result — callers must treat that as "no LDP" rather
     * than storing it.
     */
    static QString derive(double latitude, double longitude, const QString& datumSourceCS);

    /**
     * The LDP centered on \a anchorPoint, which is given in \a anchorCS's own
     * axis order and units. Converts the point to geographic coordinates on
     * \a anchorCS's own datum and hands both to derive() — the datum is the
     * anchor's, so nothing is converted between datums here.
     */
    static QString deriveFrom(const QString& anchorCS, const cwGeoPoint& anchorPoint);
};

#endif // CWLOCALPROJECTION_H
