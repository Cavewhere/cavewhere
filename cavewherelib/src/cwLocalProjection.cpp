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
     * A transform from \a horizontal to the geographic CRS \a base it is built
     * on, normalized so it reads x-first and answers longitude-first whatever
     * axis order either CRS declares — the same convention
     * cwCoordinateTransform hands its callers.
     *
     * Without the normalization the transform would take a geographic point
     * latitude-first, so a caller in the codebase's x-first convention would
     * silently transpose it. That makes a failure to normalize fatal rather
     * than something to fall back from, which is how cwCoordinateTransform
     * treats it too.
     */
    PjPtr toGeographicTransform(PJ_CONTEXT* context, PJ* horizontal, PJ* base)
    {
        PjPtr transform(proj_create_crs_to_crs_from_pj(context, horizontal, base,
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
}

QString cwLocalProjection::derive(double latitude, double longitude, const QString& datumSourceCS)
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

    const ContextPtr& context = frame->context;
    const PjPtr& base = frame->base;

    PjPtr conversion(proj_create_conversion_transverse_mercator(
                         context.get(), latitude, longitude, kScaleFactor,
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

    const char* projString = proj_as_proj_string(context.get(), projected.get(), PJ_PROJ_5, nullptr);
    if (projString != nullptr && *projString != '\0') {
        return QString::fromUtf8(projString);
    }

    // A datum with no proj-string spelling (a datum ensemble, or one that needs
    // a transformation grid) still has a WKT form, and PROJ reads either back.
    const char* wkt = proj_as_wkt(context.get(), projected.get(), PJ_WKT2_2019, nullptr);
    if (wkt != nullptr && *wkt != '\0') {
        return QString::fromUtf8(wkt);
    }

    return {};
}

QString cwLocalProjection::deriveFrom(const QString& anchorCS, const cwGeoPoint& anchorPoint)
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
    return derive(geographic.xy.y, geographic.xy.x, cs);
}

QString cwLocalProjection::datumName(const QString& cs)
{
    std::optional<GeodeticFrame> frame = resolveGeodeticFrame(cs);
    if (!frame) {
        return {};
    }

    // Forced, because WGS84 is a datum ensemble in modern PROJ and
    // proj_crs_get_datum answers nothing at all for one. The forced form names
    // the datum the ensemble stands for, which is what a reader recognizes.
    PjPtr datum(proj_crs_get_datum_forced(frame->context.get(), frame->base.get()));
    if (!datum) {
        return {};
    }

    const char* name = proj_get_name(datum.get());
    return name != nullptr ? QString::fromUtf8(name) : QString();
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
