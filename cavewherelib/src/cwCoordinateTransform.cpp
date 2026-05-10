/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCoordinateTransform.h"

//Qt includes
#include <QRegularExpression>

//PROJ includes
#include <proj.h>

//Std includes
#include <vector>

struct cwCoordinateTransform::Private
{
    PJ_CONTEXT* ctx = nullptr;
    PJ*         pj  = nullptr;
    bool        identity = false;
    QString     error;

    ~Private()
    {
        if (pj) {
            proj_destroy(pj);
        }
        if (ctx) {
            proj_context_destroy(ctx);
        }
    }
};

namespace {
    QStringList g_projSearchPaths;

    bool sameCS(const QString& a, const QString& b)
    {
        return a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive) == 0;
    }

    // Build a fresh PJ_CONTEXT with the configured search paths applied.
    // PROJ contexts don't inherit search paths from the global default, so
    // we set them explicitly on every context we create.
    PJ_CONTEXT* makeContextWithPaths()
    {
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
}

void cwCoordinateTransform::setProjSearchPaths(const QStringList& paths)
{
    g_projSearchPaths = paths;
}

void* cwCoordinateTransform::createProjContext()
{
    return makeContextWithPaths();
}

cwCoordinateTransform::cwCoordinateTransform(const QString& srcCS, const QString& dstCS)
    : m_d(std::make_unique<Private>())
{
    if (srcCS.isEmpty() || dstCS.isEmpty()) {
        m_d->error = QStringLiteral("Source or destination CS is empty");
        return;
    }

    if (sameCS(srcCS, dstCS)) {
        m_d->identity = true;
        return;
    }

    m_d->ctx = makeContextWithPaths();
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
            tls.ctx = makeContextWithPaths();
        }
        return tls.ctx;
    }
}

bool cwCoordinateTransform::isValidCS(const QString& cs)
{
    if (cs.trimmed().isEmpty()) {
        return false;
    }

    PJ_CONTEXT* ctx = validatorContext();
    if (!ctx) {
        return false;
    }

    PJ* p = proj_create(ctx, cs.toUtf8().constData());
    const bool valid = (p != nullptr);
    if (p) {
        proj_destroy(p);
    }
    return valid;
}

bool cwCoordinateTransform::isGeographic(const QString& cs)
{
    if (cs.trimmed().isEmpty()) {
        return false;
    }

    PJ_CONTEXT* ctx = validatorContext();
    if (!ctx) {
        return false;
    }

    PJ* p = proj_create(ctx, cs.toUtf8().constData());
    if (!p) {
        return false;
    }

    const PJ_TYPE type = proj_get_type(p);
    proj_destroy(p);
    return type == PJ_TYPE_GEOGRAPHIC_2D_CRS
        || type == PJ_TYPE_GEOGRAPHIC_3D_CRS
        || type == PJ_TYPE_GEOGRAPHIC_CRS;
}

QString cwCoordinateTransform::utmZoneToEpsg(int zone, bool north)
{
    if (zone < 1 || zone > 60) {
        return QString();
    }
    const int base = north ? 32600 : 32700;
    return QStringLiteral("EPSG:%1").arg(base + zone);
}

QVariantMap cwCoordinateTransform::parseCSMode(const QString& cs)
{
    QVariantMap result;
    result.insert(QStringLiteral("raw"), cs);

    const QString trimmed = cs.trimmed();
    if (trimmed.isEmpty()) {
        result.insert(QStringLiteral("mode"), QStringLiteral("local"));
        return result;
    }

    // Lat/Lon (WGS84). Cheap path: literal EPSG:4326 (case-insensitive).
    if (trimmed.compare(QStringLiteral("EPSG:4326"), Qt::CaseInsensitive) == 0) {
        result.insert(QStringLiteral("mode"), QStringLiteral("latlon"));
        return result;
    }

    // WGS84 UTM. EPSG:326NN (north) or EPSG:327NN (south), zone 01..60.
    static const QRegularExpression utmRe(
        QStringLiteral("^EPSG:(326|327)(\\d{2})$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = utmRe.match(trimmed);
    if (m.hasMatch()) {
        const int zone = m.captured(2).toInt();
        if (zone >= 1 && zone <= 60) {
            const bool north = m.captured(1) == QStringLiteral("326");
            result.insert(QStringLiteral("mode"), QStringLiteral("utm"));
            result.insert(QStringLiteral("utmZone"), zone);
            result.insert(QStringLiteral("utmNorth"), north);
            return result;
        }
    }

    result.insert(QStringLiteral("mode"), QStringLiteral("custom"));
    return result;
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

bool cwCoordinateSystem::isGeographic(const QString& cs)
{
    return cwCoordinateTransform::isGeographic(cs);
}

QString cwCoordinateSystem::utmZoneToEpsg(int zone, bool north)
{
    return cwCoordinateTransform::utmZoneToEpsg(zone, north);
}

QVariantMap cwCoordinateSystem::parseCSMode(const QString& cs)
{
    return cwCoordinateTransform::parseCSMode(cs);
}

QString cwCoordinateSystem::modeFor(const QString& cs)
{
    return cwCoordinateTransform::parseCSMode(cs).value(QStringLiteral("mode")).toString();
}

int cwCoordinateSystem::utmZoneFor(const QString& cs)
{
    const QVariantMap m = cwCoordinateTransform::parseCSMode(cs);
    return m.value(QStringLiteral("utmZone"), -1).toInt();
}

bool cwCoordinateSystem::utmNorthFor(const QString& cs)
{
    const QVariantMap m = cwCoordinateTransform::parseCSMode(cs);
    return m.value(QStringLiteral("utmNorth"), true).toBool();
}
