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

//Std includes
#include <optional>

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
 *     origin, and grows with east-west distance from it alone: at the 50 km
 *     kAnchorThresholdMeters allows it is 0.26 degrees at 30N, 0.38 at 40N and
 *     0.53 at 50N, against 1.93 at a UTM zone edge at 40N;
 *   - scale error is ~1 ppm at 10 km and ~11 ppm at 30 km, against UTM's
 *     designed worst case of ~400 ppm;
 *   - coordinates stay small enough to be float-safe on the GPU without an
 *     origin-offset hack;
 *   - it is analytic and invertible from the string alone, so any point
 *     round-trips to latitude/longitude exactly.
 *
 * The result is stored, never recomputed. Two machines with different
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
     * of \a datumSourceCS. An empty \a datumSourceCS falls back to WGS84, which
     * is what a typed coordinate means when nothing says otherwise.
     *
     * Returns "" for a latitude/longitude that isn't a location, when PROJ
     * can't express the result, or when \a datumSourceCS says something PROJ
     * can't read — that last one is not the same as saying nothing: the data is
     * on some datum we failed to identify, and quietly pinning WGS84 instead
     * would bake a datum-sized shift into a string that is never re-derived.
     * Callers must treat "" as "no LDP" rather than storing it.
     */
    static QString derive(double latitude, double longitude, const QString& datumSourceCS);

    /**
     * The LDP centered on \a anchorPoint, in \a anchorCS's units. Converts the
     * point to geographic coordinates on \a anchorCS's own datum and hands both
     * to derive() — the datum is the anchor's, so nothing is converted between
     * datums here.
     *
     * \a anchorPoint is always x-first: x is the easting or the
     * longitude and y the northing or the latitude, whatever axis order the CRS
     * itself declares. This is the convention cwCoordinateTransform hands its
     * callers, and the one every cwGeoPoint in the codebase is in — but a
     * geographic CRS's own order is latitude-first, so handing this function a
     * point in the CRS's declared order transposes the origin, and the wrong
     * frame is then stored for good.
     */
    static QString deriveFrom(const QString& anchorCS, const cwGeoPoint& anchorPoint);

    /**
     * The name of the datum \a cs is on, as PROJ spells it for a reader —
     * "North American Datum 1983", not "+datum=NAD83". A compound CRS answers
     * for its horizontal half, and a datum ensemble answers with the datum it
     * stands for rather than the ensemble's own name.
     *
     * Empty when \a cs is empty or PROJ can't read it. This is the datum the
     * project inherited from its anchor and is displayed under, never one that
     * is chosen: see the class doc.
     */
    static QString datumName(const QString& cs);

    /**
     * Where \a localCS is centered, as longitude (x) and latitude (y) in
     * degrees on \a localCS's own datum — the point derive() was handed, read
     * back out of the string it produced.
     *
     * This asks the frame where its own (0, 0) lands, which is the origin only
     * because the LDP has no false easting or northing. Handing it a CRS that
     * does have one (any UTM zone) answers for a point 500 km west of where
     * that zone is centered.
     *
     * The result is x-first, the convention every cwGeoPoint in the codebase is
     * in, rather than the latitude-first order a geographic CRS declares.
     * nullopt when \a localCS is empty or PROJ can't read it.
     */
    static std::optional<cwGeoPoint> origin(const QString& localCS);
};

#endif // CWLOCALPROJECTION_H
