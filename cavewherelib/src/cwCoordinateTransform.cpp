/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCoordinateTransform.h"
#include "cwCoordinateTransformPrivate.h"

//Qt includes
#include <QHash>
#include <QDir>

//Std includes
#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

const QString cwCoordinateTransform::Wgs84 = QStringLiteral("EPSG:4326");

namespace {
    QStringList g_projSearchPaths;

    //! Cap on the per-thread CS caches below, so browsing CSCustomDialog can't
    //! grow one unboundedly. Reached by clearing rather than evicting: the
    //! working set is a handful of systems, so a full clear costs one re-lookup
    //! each and needs no eviction order.
    constexpr int kCsCacheLimit = 256;

    bool sameCS(const QString& a, const QString& b)
    {
        return a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive) == 0;
    }
}

cwCoordinateTransformPrivate::~cwCoordinateTransformPrivate()
{
    if (pj) {
        proj_destroy(pj);
    }
    if (ctx) {
        proj_context_destroy(ctx);
    }
}

PJ_CONTEXT* cwCoordinateTransformPrivate::createContext()
{
    // PROJ contexts don't inherit search paths from the global default, so
    // we set them explicitly on every context we create.
    PJ_CONTEXT* ctx = proj_context_create();
    if (!ctx) {
        return nullptr;
    }
    if (!g_projSearchPaths.isEmpty()) {
        std::vector<QByteArray> utf8;
        utf8.reserve(g_projSearchPaths.size());
        std::vector<const char*> raw;
        raw.reserve(g_projSearchPaths.size());
        for (const QString& p : g_projSearchPaths) {
            utf8.push_back(p.toUtf8());
            raw.push_back(utf8.back().constData());
        }
        proj_context_set_search_paths(ctx, static_cast<int>(raw.size()), raw.data());
    }
    return ctx;
}

void cwCoordinateTransform::setProjSearchPaths(const QStringList& paths)
{
    g_projSearchPaths = paths;

    // Also apply to the default PROJ context. The contexts cwCoordinateTransform
    // creates pick up g_projSearchPaths in createContext(), but PROJ users that
    // hit PJ_DEFAULT_CTX directly — notably survex's cavern, which we link as
    // a library and which uses PJ_DEFAULT_CTX for its *cs validation — would
    // otherwise miss proj.db when launches don't carry PROJ_DATA in env
    // (Finder, Qt Creator, app bundles).
    if (!paths.isEmpty()) {
        std::vector<QByteArray> utf8;
        std::vector<const char*> raw;
        utf8.reserve(paths.size());
        raw.reserve(paths.size());
        for (const QString& p : paths) {
            utf8.push_back(p.toUtf8());
            raw.push_back(utf8.back().constData());
        }
        proj_context_set_search_paths(PJ_DEFAULT_CTX, static_cast<int>(raw.size()), raw.data());

        // Belt-and-suspenders: PROJ_DATA covers any PROJ-using code that
        // creates its own context after we run (env is read at context
        // creation when no programmatic paths are set on that context).
        qputenv("PROJ_DATA", paths.join(QDir::listSeparator()).toUtf8());
    }
}

cwCoordinateTransform::cwCoordinateTransform(const QString& srcCS, const QString& dstCS)
    : m_d(std::make_unique<cwCoordinateTransformPrivate>())
{
    if (srcCS.isEmpty() || dstCS.isEmpty()) {
        m_d->error = QStringLiteral("Source or destination CS is empty");
        return;
    }

    if (sameCS(srcCS, dstCS)) {
        m_d->identity = true;
        return;
    }

    m_d->ctx = cwCoordinateTransformPrivate::createContext();
    if (!m_d->ctx) {
        m_d->error = QStringLiteral("Failed to create PROJ context");
        return;
    }

    PJ* raw = proj_create_crs_to_crs(m_d->ctx,
                                     srcCS.toUtf8().constData(),
                                     dstCS.toUtf8().constData(),
                                     nullptr);
    if (!raw) {
        const int err = proj_context_errno(m_d->ctx);
        m_d->error = QStringLiteral("PROJ failed to create transform from '%1' to '%2': %3")
                         .arg(srcCS, dstCS, QString::fromUtf8(proj_errno_string(err)));
        proj_context_destroy(m_d->ctx);
        m_d->ctx = nullptr;
        return;
    }

    // Force x=easting/lon, y=northing/lat axis order regardless of CRS metadata.
    PJ* normalized = proj_normalize_for_visualization(m_d->ctx, raw);
    proj_destroy(raw);
    if (!normalized) {
        m_d->error = QStringLiteral("PROJ failed to normalize transform axis order");
        proj_context_destroy(m_d->ctx);
        m_d->ctx = nullptr;
        return;
    }
    m_d->pj = normalized;
}

cwCoordinateTransform::~cwCoordinateTransform() = default;

cwCoordinateTransform::cwCoordinateTransform(cwCoordinateTransform&& other) noexcept = default;

cwCoordinateTransform& cwCoordinateTransform::operator=(cwCoordinateTransform&& other) noexcept = default;

bool cwCoordinateTransform::isValid() const
{
    return m_d->identity || m_d->pj != nullptr;
}

bool cwCoordinateTransform::isIdentity() const
{
    return m_d->identity;
}

QString cwCoordinateTransform::errorMessage() const
{
    return m_d->error;
}

cwGeoPoint cwCoordinateTransform::transform(const cwGeoPoint& src) const
{
    if (m_d->identity || !m_d->pj) {
        return src;
    }

    PJ_COORD c;
    c.xyzt.x = src.x;
    c.xyzt.y = src.y;
    c.xyzt.z = src.z;
    c.xyzt.t = 0.0;
    const PJ_COORD r = proj_trans(m_d->pj, PJ_FWD, c);
    return cwGeoPoint(r.xyzt.x, r.xyzt.y, r.xyzt.z);
}

void cwCoordinateTransform::transformInPlace(cwGeoPoint* pts, qsizetype n) const
{
    if (m_d->identity || !m_d->pj || n <= 0 || !pts) {
        return;
    }

    // proj_trans_generic walks strided arrays in place; cwGeoPoint is three
    // contiguous doubles so we can hand it three aligned strides directly.
    const size_t sx = sizeof(cwGeoPoint);
    proj_trans_generic(m_d->pj,
                       PJ_FWD,
                       &pts[0].x, sx, static_cast<size_t>(n),
                       &pts[0].y, sx, static_cast<size_t>(n),
                       &pts[0].z, sx, static_cast<size_t>(n),
                       nullptr,   0, 0);
}

QStringList cwCoordinateTransform::commonProjectedCSList()
{
    // Curated list — covers the regions CaveWhere users most commonly survey.
    // Free-text EPSG entry remains the escape hatch for everything else.
    static const QStringList list = {
        // North America — UTM zones in WGS84
        QStringLiteral("EPSG:32610"), // UTM 10N
        QStringLiteral("EPSG:32611"), // UTM 11N
        QStringLiteral("EPSG:32612"), // UTM 12N
        QStringLiteral("EPSG:32613"), // UTM 13N
        QStringLiteral("EPSG:32614"), // UTM 14N
        QStringLiteral("EPSG:32615"), // UTM 15N
        QStringLiteral("EPSG:32616"), // UTM 16N
        QStringLiteral("EPSG:32617"), // UTM 17N
        QStringLiteral("EPSG:32618"), // UTM 18N
        QStringLiteral("EPSG:32619"), // UTM 19N
        // UK / Europe
        QStringLiteral("EPSG:27700"), // OSGB 1936 / British National Grid
        QStringLiteral("EPSG:25832"), // ETRS89 / UTM 32N
        QStringLiteral("EPSG:25833"), // ETRS89 / UTM 33N
        QStringLiteral("EPSG:2154"),  // RGF93 / Lambert-93 (France)
        // Australia
        QStringLiteral("EPSG:28350"), // GDA94 / MGA zone 50
        QStringLiteral("EPSG:28354"), // GDA94 / MGA zone 54
        QStringLiteral("EPSG:28355"), // GDA94 / MGA zone 55
    };
    return list;
}

namespace {
    // Reuse a per-thread context so QML validators (CSComboBox) can call
    // this on every keystroke without paying for proj_context_create +
    // search-path setup each time. PROJ contexts are not thread-safe, so
    // thread_local rather than a single shared context.
    struct ValidatorContext {
        PJ_CONTEXT* ctx = nullptr;
        ~ValidatorContext() { if (ctx) { proj_context_destroy(ctx); } }
    };

    PJ_CONTEXT* validatorContext()
    {
        thread_local ValidatorContext tls;
        if (!tls.ctx) {
            tls.ctx = cwCoordinateTransformPrivate::createContext();
        }
        return tls.ctx;
    }

    // The capped per-thread memo shared by the QHash-backed PROJ query caches
    // below. On overflow every entry is dropped — this cache's values come back
    // by value, so nothing escapes it. Failures are cached too: a CS the user is
    // still typing would otherwise rebuild and fail on every keystroke.
    template <typename Key, typename Value, typename Compute>
    Value cachedValue(QHash<Key, Value>& cache, const Key& key, Compute compute)
    {
        const auto it = cache.constFind(key);
        if (it != cache.constEnd()) {
            return *it;
        }
        const Value value = compute();
        if (cache.size() >= kCsCacheLimit) {
            cache.clear();
        }
        cache.insert(key, value);
        return value;
    }

    /**
     * The per-thread transform memo behind transformPoint() and domainCheck().
     * The cost it skips is documented on transformPoint(); pairs that fail to
     * build are cached too, for the same reason failures are cached above.
     *
     * std::map rather than QHash because cwCoordinateTransform is move-only with
     * no default constructor; try_emplace builds it in place. Per-thread, which
     * is also what the class's one-transform-one-thread contract wants.
     *
     * The returned reference lives only until the next call on this thread — an
     * overflowing cache clears every entry.
     */
    const cwCoordinateTransform& cachedTransform(const QString& source, const QString& dest)
    {
        thread_local std::map<std::pair<QString, QString>, cwCoordinateTransform> cache;
        const auto key = std::make_pair(source, dest);

        auto it = cache.find(key);
        if (it == cache.end()) {
            if (cache.size() >= static_cast<size_t>(kCsCacheLimit)) {
                cache.clear();
            }
            it = cache.try_emplace(key, source, dest).first;
        }
        return it->second;
    }
}

bool cwCoordinateTransform::isValidCS(const QString& cs)
{
    const QString key = cs.trimmed();
    if (key.isEmpty()) {
        return false;
    }

    // Per-thread cache, same reasoning and same cap as isGeographic below:
    // CSComboBox asks this on every keystroke, and so does
    // cwLocalProjectionManager once per fix station per edit. Without it each
    // call pays proj_create + proj_destroy.
    thread_local QHash<QString, bool> cache;
    return cachedValue(cache, key, [&key]() {
        PJ_CONTEXT* ctx = validatorContext();
        if (!ctx) {
            return false;
        }
        PJ* p = proj_create(ctx, key.toUtf8().constData());
        const bool valid = (p != nullptr);
        if (p) {
            proj_destroy(p);
        }
        return valid;
    });
}

std::optional<cwGeoPoint> cwCoordinateTransform::transformPoint(const QString& sourceCS,
                                                                const QString& destCS,
                                                                const cwGeoPoint& point)
{
    const QString source = sourceCS.trimmed();
    const QString dest = destCS.trimmed();
    if (source.isEmpty() || dest.isEmpty()) {
        return std::nullopt;
    }

    const cwCoordinateTransform& transform = cachedTransform(source, dest);
    if (!transform.isValid()) {
        return std::nullopt;
    }

    const cwGeoPoint transformed = transform.transform(point);
    if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y)) {
        return std::nullopt;
    }
    return transformed;
}

bool cwCoordinateTransform::isGeographic(const QString& cs)
{
    const QString key = cs.trimmed();
    if (key.isEmpty()) {
        return false;
    }

    // Per-thread cache: callers like cwCave::recomputeGridConvergence
    // hit this on every fix-station edit. Without the cache each call pays
    // proj_create + proj_get_type + proj_destroy. Capped like nameFor's
    // cache so CSCustomDialog browsing can't grow it unboundedly.
    thread_local QHash<QString, bool> cache;
    return cachedValue(cache, key, [&key]() {
        PJ_CONTEXT* ctx = validatorContext();
        if (!ctx) {
            return false;
        }
        PJ* p = proj_create(ctx, key.toUtf8().constData());
        if (!p) {
            return false;
        }
        const PJ_TYPE type = proj_get_type(p);
        proj_destroy(p);
        return type == PJ_TYPE_GEOGRAPHIC_2D_CRS
            || type == PJ_TYPE_GEOGRAPHIC_3D_CRS
            || type == PJ_TYPE_GEOGRAPHIC_CRS;
    });
}

namespace {
    //! domainCheck()'s cache key: the CS and the horizontal coordinate it was
    //! asked about. A struct rather than the three spelled into one string, so
    //! that a hit costs no allocation and no 17-digit double formatting — the
    //! model asks for three domain roles per row on every dataChanged.
    struct DomainKey {
        QString cs;
        double x;
        double y;

        bool operator==(const DomainKey& other) const = default;
    };

    size_t qHash(const DomainKey& key, size_t seed = 0) noexcept
    {
        return qHashMulti(seed, key.cs, key.x, key.y);
    }
}

cwCoordinateTransform::DomainCheck
cwCoordinateTransform::domainCheck(const QString& cs, const cwGeoPoint& point)
{
    const QString key = cs.trimmed();
    if (key.isEmpty()) {
        return {};
    }

    // Per-thread cache keyed by the CS and the horizontal coordinate (z is not
    // part of the domain test). revalidate() re-runs this for every fix on each
    // fix-station edit, and the model recomputes the domain roles per row on each
    // dataChanged; without the cache each call pays a fresh proj_create and
    // area-of-use lookup. The two valid flags pack into one byte. Capped like
    // isGeographic/nameFor so a long edit session can't grow it unboundedly.
    thread_local QHash<DomainKey, DomainCheck> cache;
    return cachedValue(cache, DomainKey{key, point.x, point.y}, [&]() -> DomainCheck {
        PJ_CONTEXT* ctx = validatorContext();
        if (!ctx) {
            return {};
        }

        PJ* crs = proj_create(ctx, key.toUtf8().constData());
        if (!crs) {
            return {};
        }

        double west = 0.0;
        double south = 0.0;
        double east = 0.0;
        double north = 0.0;
        const int haveArea =
            proj_get_area_of_use(ctx, crs, &west, &south, &east, &north, nullptr);
        proj_destroy(crs);

        // PROJ reports unknown bounds as -1000 and outright failure as 0. An area
        // that wraps the antimeridian (west > east) we don't try to reason about.
        // In any of these cases we can't judge the domain, so defer to the caller.
        constexpr double kUnknownBound = -1000.0;
        if (haveArea != 1
            || west <= kUnknownBound || south <= kUnknownBound
            || east <= kUnknownBound || north <= kUnknownBound
            || west > east) {
            return {};
        }

        // Inverse-project the fix into geographic lon/lat to compare against the
        // area of use. normalize_for_visualization (applied by the constructor)
        // makes the output x=lon, y=lat. Memoized per CS: the cache key above
        // includes the coordinate, so every keystroke misses it while the
        // transform it needs is the same one every time.
        const cwCoordinateTransform& toGeographic = cachedTransform(key, Wgs84);
        if (!toGeographic.isValid()) {
            return {};
        }
        const cwGeoPoint geo = toGeographic.transform(point);
        if (!std::isfinite(geo.x) || !std::isfinite(geo.y)) {
            // The coordinate can't even be inverse-projected — both horizontal
            // components are suspect.
            return {false, false};
        }

        // A generous margin absorbs legitimately surveying just past a UTM zone's
        // nominal edge; a transposed digit or wrong zone lands many multiples of
        // this far out, so the two never overlap. Longitude gates the easting,
        // latitude the northing, so the caller can point at the wrong one.
        constexpr double kDomainMarginDegrees = 5.0;
        const double lonLow = west - kDomainMarginDegrees;
        const double lonHigh = east + kDomainMarginDegrees;
        DomainCheck fields;
        fields.eastingValid = geo.x >= lonLow && geo.x <= lonHigh;
        fields.northingValid = geo.y >= south - kDomainMarginDegrees
                            && geo.y <= north + kDomainMarginDegrees;

        // Per-axis attribution only holds while the inverse projection stays near
        // the domain. A northing far past the pole wraps the longitude ~180°
        // (EPSG:32613 at 478000E/14430000N inverts to 75E/50N: a latitude that
        // still looks valid and a longitude that does not), which would tint the
        // easting for a bad northing. When an already-failing longitude is that
        // far outside, the axes can't be told apart — call both suspect rather
        // than point at the wrong cell. Gated on eastingValid being false so a
        // wide-domain CS (a geographic one spans the globe) can never be dragged
        // in. isWithinDomain() is unaffected either way: it only asks whether
        // some axis failed.
        constexpr double kMaxAttributableLonExcessDegrees = 90.0;
        if (!fields.eastingValid) {
            const auto angularDistance = [](double a, double b) {
                const double delta = std::fmod(std::abs(a - b), 360.0);
                return delta > 180.0 ? 360.0 - delta : delta;
            };
            const double excess = (std::min)(angularDistance(geo.x, lonLow),
                                             angularDistance(geo.x, lonHigh));
            if (excess > kMaxAttributableLonExcessDegrees) {
                return {false, false};
            }
        }
        return fields;
    });
}

QString cwCoordinateTransform::utmZoneToEpsg(int zone, bool north)
{
    if (zone < 1 || zone > 60) {
        return QString();
    }
    const int base = north ? 32600 : 32700;
    return QStringLiteral("EPSG:%1").arg(base + zone);
}

QString cwCoordinateTransform::deriveProjectedOutputCS(const QString& inputCS,
                                                       const cwGeoPoint& point)
{
    const QString cs = inputCS.trimmed();
    if (cs.isEmpty() || !isValidCS(cs)) {
        return QString();
    }
    if (!isGeographic(cs)) {
        // Already projected — usable as the output CS verbatim.
        return cs;
    }

    // A geographic input can't be the output CS; pick the WGS84 UTM zone that
    // contains the fix. transformPoint memoizes the transform per thread,
    // normalizes the axis order to x=longitude, y=latitude, and rejects
    // non-finite results.
    const auto geo = transformPoint(cs, Wgs84, point);
    if (!geo.has_value()) {
        return QString();
    }

    constexpr double kDegreesPerZone = 6.0;
    constexpr double kZoneOriginLongitude = 180.0;
    const int rawZone = int(std::floor((geo->x + kZoneOriginLongitude) / kDegreesPerZone)) + 1;
    const int zone = qBound(1, rawZone, 60);
    const bool north = geo->y >= 0.0;
    return utmZoneToEpsg(zone, north);
}

namespace {
    struct ParsedCS {
        cwCoordinateSystem::Mode mode = cwCoordinateSystem::Local;
        int  utmZone  = -1;
        bool utmNorth = true;
    };

    ParsedCS parseCS(const QString& cs)
    {
        ParsedCS r;
        const QString trimmed = cs.trimmed();
        if (trimmed.isEmpty()) {
            return r;
        }

        if (trimmed.compare(cwCoordinateTransform::Wgs84, Qt::CaseInsensitive) == 0) {
            r.mode = cwCoordinateSystem::LatLon;
            return r;
        }

        if (trimmed.startsWith(QStringLiteral("EPSG:"), Qt::CaseInsensitive)) {
            bool ok = false;
            const int code = trimmed.mid(5).toInt(&ok);
            if (ok) {
                if (code >= 32601 && code <= 32660) {
                    r.mode = cwCoordinateSystem::UTM;
                    r.utmZone = code - 32600;
                    r.utmNorth = true;
                    return r;
                }
                if (code >= 32701 && code <= 32760) {
                    r.mode = cwCoordinateSystem::UTM;
                    r.utmZone = code - 32700;
                    r.utmNorth = false;
                    return r;
                }
            }
        }

        r.mode = cwCoordinateSystem::Custom;
        return r;
    }
}

QString cwCoordinateTransform::nameFor(const QString& cs)
{
    const QString key = cs.trimmed();
    if (key.isEmpty()) {
        return QString();
    }

    // Per-thread cache: QML label bindings hit the same CS string repeatedly
    // (one cache entry covers every fix-station row using the same projection),
    // and proj_create is non-trivial — sqlite query + CRS construction. Pairs
    // with the thread_local validatorContext() above; no mutex needed. Capped
    // because CSCustomDialog lets users browse all ~7000 EPSG entries.
    thread_local QHash<QString, QString> cache;
    return cachedValue(cache, key, [&key]() {
        QString result;
        PJ_CONTEXT* ctx = validatorContext();
        if (!ctx) {
            return result;
        }
        PJ* p = proj_create(ctx, key.toUtf8().constData());
        if (p) {
            const char* name = proj_get_name(p);
            if (name) {
                result = QString::fromUtf8(name);
            }
            proj_destroy(p);
        }
        return result;
    });
}

// ---- cwCoordinateSystem (QML singleton facade) ----

bool cwCoordinateSystem::isValidCS(const QString& cs)
{
    return cwCoordinateTransform::isValidCS(cs);
}

QStringList cwCoordinateSystem::commonProjectedCSList()
{
    return cwCoordinateTransform::commonProjectedCSList();
}

QString cwCoordinateSystem::wgs84()
{
    return cwCoordinateTransform::Wgs84;
}

bool cwCoordinateSystem::isGeographic(const QString& cs)
{
    return cwCoordinateTransform::isGeographic(cs);
}

QString cwCoordinateSystem::utmZoneToEpsg(int zone, bool north)
{
    return cwCoordinateTransform::utmZoneToEpsg(zone, north);
}

cwCoordinateSystem::Mode cwCoordinateSystem::modeFor(const QString& cs)
{
    return parseCS(cs).mode;
}

int cwCoordinateSystem::utmZoneFor(const QString& cs)
{
    return parseCS(cs).utmZone;
}

bool cwCoordinateSystem::utmNorthFor(const QString& cs)
{
    return parseCS(cs).utmNorth;
}

QString cwCoordinateSystem::nameFor(const QString& cs)
{
    return cwCoordinateTransform::nameFor(cs);
}
