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

    //! The geographic CRS \a horizontal is built on — the datum the LDP inherits.
    PjPtr geodeticBase(PJ_CONTEXT* context, PJ* horizontal)
    {
        PjPtr base(proj_crs_get_geodetic_crs(context, horizontal));
        if (!base) {
            // Already geographic: it is its own base.
            base = PjPtr(proj_clone(context, horizontal));
        }
        if (!base) {
            return {};
        }

        PjPtr twoDimensional(proj_crs_demote_to_2D(context, nullptr, base.get()));
        return twoDimensional ? std::move(twoDimensional) : std::move(base);
    }
}

QString cwLocalProjection::derive(double latitude, double longitude, const QString& datumSourceCS)
{
    if (!std::isfinite(latitude) || !std::isfinite(longitude)
        || std::abs(latitude) > kMaxLatitude || std::abs(longitude) > kMaxLongitude) {
        return {};
    }

    ContextPtr context(cwCoordinateTransformPrivate::createContext());
    if (!context) {
        return {};
    }

    PjPtr horizontal = horizontalCrs(context.get(), datumSourceCS);
    if (!horizontal) {
        // Nothing said which datum this is, or PROJ couldn't read what did.
        // WGS84 is what a typed coordinate means when nothing says otherwise.
        horizontal = horizontalCrs(context.get(), cwCoordinateTransform::Wgs84);
    }
    if (!horizontal) {
        return {};
    }

    PjPtr base = geodeticBase(context.get(), horizontal.get());
    if (!base) {
        return {};
    }

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

    ContextPtr context(cwCoordinateTransformPrivate::createContext());
    if (!context) {
        return {};
    }

    PjPtr horizontal = horizontalCrs(context.get(), cs);
    if (!horizontal) {
        return {};
    }

    PjPtr base = geodeticBase(context.get(), horizontal.get());
    if (!base) {
        return {};
    }

    PjPtr toGeographic(proj_create_crs_to_crs_from_pj(context.get(), horizontal.get(),
                                                      base.get(), nullptr, nullptr));
    if (!toGeographic) {
        return {};
    }

    // Normalized, so the answer is longitude-first whatever the CRS's own axis
    // order is — the same convention cwCoordinateTransform hands its callers.
    PjPtr normalized(proj_normalize_for_visualization(context.get(), toGeographic.get()));
    if (normalized) {
        toGeographic = std::move(normalized);
    }

    const PJ_COORD source = proj_coord(anchorPoint.x, anchorPoint.y, 0.0, 0.0);
    const PJ_COORD geographic = proj_trans(toGeographic.get(), PJ_FWD, source);

    // proj_trans reports failure as HUGE_VAL, which derive() rejects.
    return derive(geographic.xy.y, geographic.xy.x, cs);
}
