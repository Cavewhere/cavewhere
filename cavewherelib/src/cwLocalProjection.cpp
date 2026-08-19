/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLocalProjection.h"
#include "cwCoordinateTransformPrivate.h"

//PROJ includes — building a CRS rather than transforming through one is still
//in PROJ's experimental C API, which lives in its own header.
#include <proj_experimental.h>

//Std includes
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace {

    constexpr const char* kProjectionName = "CaveWhere local projection";
    constexpr const char* kAngularUnitName = "Degree";
    constexpr double kRadiansPerDegree = 0.017453292519943295;
    constexpr const char* kLinearUnitName = "Metre";
    constexpr double kMetersPerLinearUnit = 1.0;

    //! k_0 = 1, forever. Tuning it to the project's elevation buys ~16 ppm per
    //! 100 m — below survey noise — at the cost of a frame that would have to
    //! move when the elevation estimate changed.
    constexpr double kScaleFactor = 1.0;

    //! x_0 = y_0 = 0: the anchor is the origin, so scene coordinates are LDP
    //! coordinates with nothing subtracted.
    constexpr double kFalseOrigin = 0.0;

    constexpr double kMaxLatitude = 90.0;
    constexpr double kMaxLongitude = 180.0;

    //! One part of the world and the plate-fixed geodetic CRS customary there.
    //! The box is coarse on purpose: it decides a datum, and neighboring national
    //! frames agree to within centimeters along the borders they share.
    struct PlateFixedRegion {
        double minLatitude;
        double maxLatitude;
        double minLongitude;
        double maxLongitude;
        const char* coordinateSystem;
    };

    /**
     * Where each national plate-fixed frame applies, first match winning.
     *
     * These frames are tied to their own plate, so a cave keeps the coordinates
     * it was surveyed on; WGS84 is tied to the whole Earth and slides under North
     * America by about 2 cm a year.
     *
     * The United States comes first, so the strips it shares with Canada and
     * Mexico resolve to NAD83(2011) — the two answers there differ by a few
     * centimeters, and a box drawn along the real border would still be a guess
     * about which side of it a cave is on. Alaska takes two rows so that its
     * main box stops at the 141st meridian, the border it shares with the
     * Yukon, and the panhandle is the strip east of it: one box out to -129
     * would reach Whitehorse and most of the Yukon, which is inland Canada
     * rather than a shared border.
     *
     * Europe takes four rows because ETRS89 is tied to the stable part of the
     * Eurasian plate and stops where Europe does: the boxes are drawn to leave
     * out North Africa (Morocco reaches 35.9N, Algeria 37.1N and Tunisia 37.4N)
     * and Anatolia, which are on plates of their own and have national frames
     * of their own. The southern Spanish coast and the Aegean islands fall
     * outside them and keep WGS84, which is the modest answer rather than a
     * wrong one.
     *
     * These frames get replaced on decade scales (NAD83 → NATRF2022 is coming).
     * A changed entry only reaches frames derived after it changed, because a
     * stored frame is never re-derived.
     */
    constexpr PlateFixedRegion kPlateFixedRegions[] = {
        {  24.5,  49.5, -125.0,  -66.5, "EPSG:6318" },  // Conterminous US, NAD83(2011)
        {  51.0,  72.0, -173.0, -141.0, "EPSG:6318" },  // Alaska west of the Yukon border
        {  54.5,  60.5, -141.0, -129.5, "EPSG:6318" },  // The Alaskan panhandle
        {  18.0,  23.0, -161.0, -154.0, "EPSG:6318" },  // Hawaii
        {  17.5,  18.6,  -68.0,  -64.5, "EPSG:6318" },  // Puerto Rico and the Virgin Islands
        {  41.5,  84.0, -141.0,  -52.0, "EPSG:4617" },  // Canada, NAD83(CSRS)
        {  14.0,  33.0, -118.0,  -86.0, "EPSG:6365" },  // Mexico ITRF2008
        {  36.0,  72.0,  -12.0,   -1.0, "EPSG:4258" },  // Iberia and the British Isles, ETRS89
        {  37.5,  72.0,   -1.0,   12.0, "EPSG:4258" },  // France to western Italy and Scandinavia
        {  34.0,  42.0,   12.0,   26.0, "EPSG:4258" },  // Sicily, the Adriatic and Greece
        {  42.0,  72.0,   12.0,   40.0, "EPSG:4258" },  // Central and eastern Europe
        {  24.0,  46.0,  122.0,  154.0, "EPSG:6668" },  // Japan, JGD2011
        { -44.0,  -9.0,  112.0,  154.0, "EPSG:7844" },  // Australia, GDA2020
        { -48.0, -33.0,  166.0,  179.0, "EPSG:4167" },  // New Zealand, NZGD2000
    };

    //! The plate-fixed geodetic CRS customary where (\a latitude, \a longitude)
    //! is, or "" where no entry covers it — the caller then keeps WGS84. A frame
    //! has to pick one, so it takes the first of the datums the boxes offer.
    QString plateFixedDatumFor(double latitude, double longitude)
    {
        return cwLocalProjection::plateFixedDatumsFor(latitude, longitude).value(0);
    }

    struct PjDeleter {
        void operator()(PJ* pj) const { proj_destroy(pj); }
    };
    using PjPtr = std::unique_ptr<PJ, PjDeleter>;

    struct ContextDeleter {
        void operator()(PJ_CONTEXT* context) const { proj_context_destroy(context); }
    };
    using ContextPtr = std::unique_ptr<PJ_CONTEXT, ContextDeleter>;

    /**
     * The horizontal, two-dimensional half of \a cs. A compound CRS — what a
     * lidar tile declares — contributes only its horizontal component: its
     * vertical half must never reach a transform, or PROJ applies the geoid
     * model and silently returns ellipsoidal heights on machines that happen to
     * have the grids installed.
     */
    PjPtr horizontalCrs(PJ_CONTEXT* context, const QString& cs)
    {
        PjPtr crs(proj_create(context, cs.trimmed().toUtf8().constData()));
        if (!crs) {
            return {};
        }

        if (proj_get_type(crs.get()) == PJ_TYPE_COMPOUND_CRS) {
            PjPtr horizontal(proj_crs_get_sub_crs(context, crs.get(), 0));
            if (!horizontal) {
                return {};
            }
            crs = std::move(horizontal);
        }

        PjPtr twoDimensional(proj_crs_demote_to_2D(context, nullptr, crs.get()));
        return twoDimensional ? std::move(twoDimensional) : std::move(crs);
    }

    //! Whether \a object is a CRS the LDP can be built on top of — one that
    //! carries a datum and an ellipsoid.
    bool isGeodeticCrs(PJ* object)
    {
        switch (proj_get_type(object)) {
        case PJ_TYPE_GEODETIC_CRS:
        case PJ_TYPE_GEOCENTRIC_CRS:
        case PJ_TYPE_GEOGRAPHIC_CRS:
        case PJ_TYPE_GEOGRAPHIC_2D_CRS:
        case PJ_TYPE_GEOGRAPHIC_3D_CRS:
            return true;
        default:
            return false;
        }
    }

    //! The geographic CRS \a horizontal is built on — the datum the LDP inherits.
    PjPtr geodeticBase(PJ_CONTEXT* context, PJ* horizontal)
    {
        PjPtr base(proj_crs_get_geodetic_crs(context, horizontal));
        if (!base) {
            // Either it is already geographic and is its own base, or it isn't a
            // CRS at all — proj_create accepts a bare "+proj=utm +zone=16" as a
            // transformation, and a transformation cloned into the base slot
            // would put a projection on top of something with no datum.
            if (!isGeodeticCrs(horizontal)) {
                return {};
            }
            base = PjPtr(proj_clone(context, horizontal));
        }
        if (!base) {
            return {};
        }

        PjPtr twoDimensional(proj_crs_demote_to_2D(context, nullptr, base.get()));
        return twoDimensional ? std::move(twoDimensional) : std::move(base);
    }

    /**
     * A transform from \a source to the geographic CRS \a geographic,
     * normalized so it reads x-first and answers longitude-first whatever axis
     * order either CRS declares — the same convention cwCoordinateTransform
     * hands its callers.
     *
     * Without the normalization the transform would take a geographic point
     * latitude-first, so a caller in the codebase's x-first convention would
     * silently transpose it. That makes a failure to normalize fatal rather
     * than something to fall back from, which is how cwCoordinateTransform
     * treats it too.
     *
     * Both CRSes must have been created in \a context, which is what a PJ
     * carries its thread affinity through.
     */
    PjPtr toGeographicTransform(PJ_CONTEXT* context, PJ* source, PJ* geographic)
    {
        PjPtr transform(proj_create_crs_to_crs_from_pj(context, source, geographic,
                                                       nullptr, nullptr));
        if (!transform) {
            return {};
        }

        return PjPtr(proj_normalize_for_visualization(context, transform.get()));
    }

    /**
     * A coordinate system resolved down to the pieces every entry point here
     * needs: the context they were created in, the horizontal half of \a cs, and
     * the geographic CRS that half is built on.
     *
     * The context is declared first so it outlives the two PJs it owns.
     */
    struct GeodeticFrame {
        ContextPtr context;
        PjPtr horizontal;
        PjPtr base;
    };

    //! \a cs resolved, or nullopt when PROJ can't read it as something with a datum.
    std::optional<GeodeticFrame> resolveGeodeticFrame(const QString& cs)
    {
        ContextPtr context(cwCoordinateTransformPrivate::createContext());
        if (!context) {
            return std::nullopt;
        }

        PjPtr horizontal = horizontalCrs(context.get(), cs);
        if (!horizontal) {
            return std::nullopt;
        }

        PjPtr base = geodeticBase(context.get(), horizontal.get());
        if (!base) {
            return std::nullopt;
        }

        return GeodeticFrame{std::move(context), std::move(horizontal), std::move(base)};
    }

    /**
     * How PROJ names the datum \a base is on, for a reader.
     *
     * Forced, because WGS84 is a datum ensemble in modern PROJ and
     * proj_crs_get_datum answers nothing at all for one. The forced form names
     * the datum the ensemble stands for, which is what a reader recognizes.
     */
    QString forcedDatumName(PJ_CONTEXT* context, PJ* base)
    {
        PjPtr datum(proj_crs_get_datum_forced(context, base));
        if (!datum) {
            return {};
        }

        const char* name = proj_get_name(datum.get());
        return name != nullptr ? QString::fromUtf8(name) : QString();
    }

    //! Whether \a base is on the WGS84 ensemble itself, as opposed to one of its
    //! realizations — someone who typed a realization meant that realization.
    bool isWgs84Ensemble(PJ_CONTEXT* context, PJ* base)
    {
        // A realization answers a longer name of its own ("World Geodetic System
        // 1984 (G1762)"), so matching the whole string leaves realizations alone.
        return forcedDatumName(context, base) == QStringLiteral("World Geodetic System 1984");
    }

    /**
     * Move \a frame and the origin (\a latitude, \a longitude) onto the
     * plate-fixed datum customary where that origin is, when the frame is on the
     * WGS84 ensemble and the table covers the place. Leaves both exactly as they
     * were otherwise.
     *
     * Failure runs the opposite way from derive()'s rule about a declared system.
     * The declared one is what the user said, so an unreadable one is refused;
     * this is CaveWhere's own suggestion, so a CS that won't resolve or a point
     * that won't transform keeps the WGS84 frame rather than costing the user a
     * frame at all.
     *
     * The origin travels across rather than being reused as typed, which keeps
     * the frame's origin the anchor's position on the frame's own datum — the
     * ~1.4 m the ensemble differs by is irrelevant to centering and load-bearing
     * for what origin() promises to read back.
     */
    void adoptPlateFixedDatum(GeodeticFrame& frame, double& latitude, double& longitude)
    {
        PJ_CONTEXT* context = frame.context.get();
        if (!isWgs84Ensemble(context, frame.base.get())) {
            return;
        }

        // The WGS84 lat/long picks the box directly: the ensemble offset is
        // meters, and no box edge is drawn that finely.
        const QString plateCS = plateFixedDatumFor(latitude, longitude);
        if (plateCS.isEmpty()) {
            return;
        }

        // Resolved in the frame's own context, because the transform below can
        // only relate two CRSes that were created in one.
        PjPtr plateHorizontal = horizontalCrs(context, plateCS);
        if (!plateHorizontal) {
            return;
        }

        PjPtr plateBase = geodeticBase(context, plateHorizontal.get());
        if (!plateBase) {
            return;
        }

        PjPtr toPlate = toGeographicTransform(context, frame.base.get(), plateBase.get());
        if (!toPlate) {
            return;
        }

        // Normalized, so this is x-first on both ends: x is the longitude.
        const PJ_COORD source = proj_coord(longitude, latitude, 0.0, 0.0);
        const PJ_COORD plate = proj_trans(toPlate.get(), PJ_FWD, source);

        // proj_trans reports failure as HUGE_VAL.
        if (!std::isfinite(plate.xy.x) || !std::isfinite(plate.xy.y)) {
            return;
        }

        longitude = plate.xy.x;
        latitude = plate.xy.y;
        frame.horizontal = std::move(plateHorizontal);
        frame.base = std::move(plateBase);
    }
}

QString cwLocalProjection::derive(double latitude, double longitude, const QString& datumSourceCS,
                                  DatumSource datumSource)
{
    if (!std::isfinite(latitude) || !std::isfinite(longitude)
        || std::abs(latitude) > kMaxLatitude || std::abs(longitude) > kMaxLongitude) {
        return {};
    }

    // Nothing said which datum this is: WGS84 is what a typed coordinate means
    // when nothing says otherwise. A system that says something PROJ can't read
    // is the opposite case and is refused — the data is on some datum we failed
    // to identify, and pinning WGS84 anyway would bake a datum-sized shift into
    // a frame that is never re-derived.
    const QString datumCS = datumSourceCS.trimmed().isEmpty() ? cwCoordinateTransform::Wgs84
                                                              : datumSourceCS;

    std::optional<GeodeticFrame> frame = resolveGeodeticFrame(datumCS);
    if (!frame) {
        return {};
    }

    // A plain lat/long carries no plate with it, so a frame derived from data
    // gets the one its part of the world holds still against. Anything with a
    // datum of its own — including a named WGS84 realization — passes through
    // untouched, and so does a frame being recentered: the datum was settled
    // when the project was placed, and moving the origin is about position.
    double originLatitude = latitude;
    double originLongitude = longitude;
    if (datumSource == DatumSource::DataInput) {
        adoptPlateFixedDatum(*frame, originLatitude, originLongitude);
    }

    const ContextPtr& context = frame->context;
    const PjPtr& base = frame->base;

    PjPtr conversion(proj_create_conversion_transverse_mercator(
                         context.get(), originLatitude, originLongitude, kScaleFactor,
                         kFalseOrigin, kFalseOrigin,
                         kAngularUnitName, kRadiansPerDegree,
                         kLinearUnitName, kMetersPerLinearUnit));
    if (!conversion) {
        return {};
    }

    PjPtr cartesian(proj_create_cartesian_2D_cs(context.get(), PJ_CART2D_EASTING_NORTHING,
                                                kLinearUnitName, kMetersPerLinearUnit));
    if (!cartesian) {
        return {};
    }

    PjPtr projected(proj_create_projected_crs(context.get(), kProjectionName, base.get(),
                                              conversion.get(), cartesian.get()));
    if (!projected) {
        return {};
    }

    // Preferred for how short and how readable it is, but only when the datum
    // survives it. proj_as_proj_string has keywords for a handful of datums and
    // drops the rest rather than refusing, so all that comes back of NAD83(2011)
    // is "+ellps=GRS80" — a frame that names no datum at all, stored for good.
    const QString frameDatum = forcedDatumName(context.get(), base.get());
    const char* projString = proj_as_proj_string(context.get(), projected.get(), PJ_PROJ_5, nullptr);
    if (projString != nullptr && *projString != '\0') {
        const QString spelling = QString::fromUtf8(projString);
        if (cwLocalProjection::datumName(spelling) == frameDatum) {
            return spelling;
        }
    }

    // A datum with no proj-string spelling (a datum ensemble, a national
    // realization, or one that needs a transformation grid) still has a WKT
    // form, and PROJ reads either back.
    const char* wkt = proj_as_wkt(context.get(), projected.get(), PJ_WKT2_2019, nullptr);
    if (wkt != nullptr && *wkt != '\0') {
        return QString::fromUtf8(wkt);
    }

    return {};
}

QString cwLocalProjection::deriveFrom(const QString& anchorCS, const cwGeoPoint& anchorPoint,
                                      DatumSource datumSource)
{
    if (!std::isfinite(anchorPoint.x) || !std::isfinite(anchorPoint.y)) {
        return {};
    }

    const QString cs = anchorCS.trimmed().isEmpty() ? cwCoordinateTransform::Wgs84
                                                    : anchorCS.trimmed();

    std::optional<GeodeticFrame> frame = resolveGeodeticFrame(cs);
    if (!frame) {
        return {};
    }

    PjPtr toGeographic = toGeographicTransform(frame->context.get(), frame->horizontal.get(),
                                               frame->base.get());
    if (!toGeographic) {
        return {};
    }

    const PJ_COORD source = proj_coord(anchorPoint.x, anchorPoint.y, 0.0, 0.0);
    const PJ_COORD geographic = proj_trans(toGeographic.get(), PJ_FWD, source);

    // proj_trans reports failure as HUGE_VAL, which derive() rejects.
    return derive(geographic.xy.y, geographic.xy.x, cs, datumSource);
}

QString cwLocalProjection::datumName(const QString& cs)
{
    std::optional<GeodeticFrame> frame = resolveGeodeticFrame(cs);
    if (!frame) {
        return {};
    }

    return forcedDatumName(frame->context.get(), frame->base.get());
}

std::optional<cwGeoPoint> cwLocalProjection::origin(const QString& localCS)
{
    std::optional<GeodeticFrame> frame = resolveGeodeticFrame(localCS);
    if (!frame) {
        return std::nullopt;
    }

    PjPtr toGeographic = toGeographicTransform(frame->context.get(), frame->horizontal.get(),
                                               frame->base.get());
    if (!toGeographic) {
        return std::nullopt;
    }

    const PJ_COORD center = proj_coord(kFalseOrigin, kFalseOrigin, 0.0, 0.0);
    const PJ_COORD geographic = proj_trans(toGeographic.get(), PJ_FWD, center);

    // proj_trans reports failure as HUGE_VAL.
    if (!std::isfinite(geographic.xy.x) || !std::isfinite(geographic.xy.y)) {
        return std::nullopt;
    }

    return cwGeoPoint(geographic.xy.x, geographic.xy.y, 0.0);
}

QStringList cwLocalProjection::plateFixedDatumsFor(double latitude, double longitude)
{
    QStringList datums;
    for (const PlateFixedRegion& region : kPlateFixedRegions) {
        const bool contains = latitude >= region.minLatitude && latitude <= region.maxLatitude
            && longitude >= region.minLongitude && longitude <= region.maxLongitude;
        if (!contains) {
            continue;
        }
        const QString datum = QString::fromLatin1(region.coordinateSystem);
        // One datum spans several boxes (the US takes five), and a point can
        // fall in two of them, so the same answer must reach the caller once.
        if (!datums.contains(datum)) {
            datums.append(datum);
        }
    }
    return datums;
}
