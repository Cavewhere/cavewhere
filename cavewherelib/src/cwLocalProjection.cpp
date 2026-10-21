/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLocalProjection.h"
#include "cwCoordinateTransformPrivate.h"
#include "cwDatumCatalog.h"

//PROJ includes — building a CRS rather than transforming through one is still
//in PROJ's experimental C API, which lives in its own header.
#include <proj_experimental.h>

//Std includes
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

    //! Where \a frame's own (0, 0) lands, in degrees and x-first — see
    //! origin(), whose whole promise this is.
    std::optional<cwGeoPoint> originOf(const GeodeticFrame& frame)
    {
        PjPtr toGeographic = toGeographicTransform(frame.context.get(), frame.horizontal.get(),
                                                   frame.base.get());
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

namespace {

    //! The name a translated conversion carries; nothing reads it back.
    constexpr const char* kTranslatedConversionName = "CaveWhere local grid translation";

    //! EPSG's parameter codes for a projection's false origin, in each of the
    //! forms its methods name it.
    constexpr QLatin1StringView kEpsgFalseEasting("8806");
    constexpr QLatin1StringView kEpsgEastingAtProjectionCentre("8816");
    constexpr QLatin1StringView kEpsgEastingAtFalseOrigin("8826");
    constexpr QLatin1StringView kEpsgFalseNorthing("8807");
    constexpr QLatin1StringView kEpsgNorthingAtProjectionCentre("8817");
    constexpr QLatin1StringView kEpsgNorthingAtFalseOrigin("8827");

    bool isFalseEastingCode(QLatin1StringView code)
    {
        return code == kEpsgFalseEasting
            || code == kEpsgEastingAtProjectionCentre
            || code == kEpsgEastingAtFalseOrigin;
    }

    bool isFalseNorthingCode(QLatin1StringView code)
    {
        return code == kEpsgFalseNorthing
            || code == kEpsgNorthingAtProjectionCentre
            || code == kEpsgNorthingAtFalseOrigin;
    }

    //! PROJ's unit category, as the unit type proj_create_conversion takes.
    PJ_UNIT_TYPE unitTypeOf(const char* category)
    {
        const QLatin1StringView name(category != nullptr ? category : "");
        if (name == QLatin1StringView("linear")) {
            return PJ_UT_LINEAR;
        }
        if (name == QLatin1StringView("angular")) {
            return PJ_UT_ANGULAR;
        }
        if (name == QLatin1StringView("scale")) {
            return PJ_UT_SCALE;
        }
        if (name == QLatin1StringView("time")) {
            return PJ_UT_TIME;
        }
        return PJ_UT_PARAMETRIC;
    }

    /**
     * The projected CRS \a conversion builds on \a base in \a cartesian, as
     * text PROJ reads back: a PROJ string when the datum survives one, else
     * WKT. Empty when PROJ can't build or spell it.
     */
    QString spellProjected(PJ_CONTEXT* context, PJ* base, PJ* conversion, PJ* cartesian)
    {
        PjPtr projected(proj_create_projected_crs(context, kProjectionName, base,
                                                  conversion, cartesian));
        if (!projected) {
            return {};
        }

        // Preferred for how short and how readable it is, but only when the
        // datum survives it. proj_as_proj_string has keywords for a handful of
        // datums and drops the rest rather than refusing, so all that comes
        // back of NAD83(2011) is "+ellps=GRS80" — a frame that names no datum
        // at all, stored for good.
        const QString frameDatum = forcedDatumName(context, base);
        const char* projString = proj_as_proj_string(context, projected.get(), PJ_PROJ_5, nullptr);
        if (projString != nullptr && *projString != '\0') {
            const QString spelling = QString::fromUtf8(projString);
            if (cwLocalProjection::datumName(spelling) == frameDatum) {
                return spelling;
            }
        }

        // A datum with no proj-string spelling (a datum ensemble, a national
        // realization, or one that needs a transformation grid) still has a
        // WKT form, and PROJ reads either back.
        const char* wkt = proj_as_wkt(context, projected.get(), PJ_WKT2_2019, nullptr);
        if (wkt != nullptr && *wkt != '\0') {
            return QString::fromUtf8(wkt);
        }

        return {};
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

    return spellProjected(context.get(), base.get(), conversion.get(), cartesian.get());
}

QString cwLocalProjection::translated(const QString& frameCS, const cwGeoPoint& origin)
{
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) {
        return {};
    }

    std::optional<GeodeticFrame> frame = resolveGeodeticFrame(frameCS);
    if (!frame || proj_get_type(frame->horizontal.get()) != PJ_TYPE_PROJECTED_CRS) {
        return {};
    }
    PJ_CONTEXT* context = frame->context.get();

    PjPtr conversion(proj_crs_get_coordoperation(context, frame->horizontal.get()));
    PjPtr cartesian(proj_crs_get_coordinate_system(context, frame->horizontal.get()));
    if (!conversion || !cartesian) {
        return {};
    }

    const char* methodName = nullptr;
    const char* methodAuthority = nullptr;
    const char* methodCode = nullptr;
    if (!proj_coordoperation_get_method_info(context, conversion.get(), &methodName,
                                             &methodAuthority, &methodCode)) {
        return {};
    }

    // The frame's own parameters, with its false origin moved by \a origin.
    // The strings the descriptions point at belong to the conversion, which
    // outlives them.
    const int parameterCount = proj_coordoperation_get_param_count(context, conversion.get());
    std::vector<PJ_PARAM_DESCRIPTION> parameters;
    parameters.reserve(static_cast<size_t>(std::max(parameterCount, 0)));
    bool movedEasting = false;
    bool movedNorthing = false;
    for (int index = 0; index < parameterCount; ++index) {
        PJ_PARAM_DESCRIPTION parameter{};
        const char* unitAuthority = nullptr;
        const char* unitCode = nullptr;
        const char* unitCategory = nullptr;
        double unitToSi = 0.0;
        if (!proj_coordoperation_get_param(context, conversion.get(), index, &parameter.name,
                                           &parameter.auth_name, &parameter.code,
                                           &parameter.value, nullptr, &unitToSi,
                                           &parameter.unit_name, &unitAuthority, &unitCode,
                                           &unitCategory)) {
            return {};
        }
        parameter.unit_conv_factor = unitToSi;
        parameter.unit_type = unitTypeOf(unitCategory);

        const QLatin1StringView code(parameter.code != nullptr ? parameter.code : "");
        if (unitToSi > 0.0 && isFalseEastingCode(code)) {
            parameter.value += origin.x / unitToSi;
            movedEasting = true;
        } else if (unitToSi > 0.0 && isFalseNorthingCode(code)) {
            parameter.value += origin.y / unitToSi;
            movedNorthing = true;
        }
        parameters.push_back(parameter);
    }
    if (!movedEasting || !movedNorthing) {
        return {};
    }

    PjPtr moved(proj_create_conversion(context, kTranslatedConversionName, nullptr, nullptr,
                                       methodName, methodAuthority, methodCode,
                                       parameterCount, parameters.data()));
    if (!moved) {
        return {};
    }

    return spellProjected(context, frame->base.get(), moved.get(), cartesian.get());
}

QString cwLocalProjection::localGridSystem()
{
    // The system never reaches the user, so the WGS84 ensemble it stands on is
    // no datum anyone is placed on.
    static const QString system =
        derive(0.0, 0.0, cwCoordinateTransform::Wgs84, DatumSource::StoredFrame);
    return system;
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

cwLocalProjection::Description cwLocalProjection::describe(const QString& cs)
{
    std::optional<GeodeticFrame> frame = resolveGeodeticFrame(cs);
    if (!frame) {
        return {};
    }

    return Description{forcedDatumName(frame->context.get(), frame->base.get()),
                       originOf(*frame)};
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

    return originOf(*frame);
}

QStringList cwLocalProjection::plateFixedDatumsFor(double latitude, double longitude)
{
    const auto contains = [latitude, longitude](const cwDatumCatalog::Region& region) {
        return latitude >= region.minLatitude && latitude <= region.maxLatitude
            && longitude >= region.minLongitude && longitude <= region.maxLongitude;
    };

    // One datum spans several boxes (the US takes five), and a point can fall in
    // two of them; one append per datum row keeps each answer to once.
    QStringList datums;
    for (const cwDatumCatalog::Datum& datum : cwDatumCatalog::kDatums) {
        if (std::ranges::any_of(datum.regions, contains)) {
            datums.append(QString::fromLatin1(datum.geographicCode));
        }
    }
    return datums;
}
