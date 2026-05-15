/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGridConvergence.h"
#include "cwCoordinateTransform.h"
#include "cwCoordinateTransformPrivate.h"

//Qt includes
#include <QHash>
#include <QtMath>

//Std includes
#include <memory>

namespace {

// Per-sourceCS PJ cached on the calling thread. proj_factors() is the
// expensive call (sqlite hit during proj_create plus CRS construction),
// and convergence is read on every CavePage render — caching keeps it
// effectively free on the second hit. PJ_CONTEXT is per-instance and
// not thread-safe, so thread_local mirrors cwCoordinateTransform's
// validatorContext() pattern.
struct ConvergencePj {
    PJ_CONTEXT* ctx = nullptr;
    PJ*         pj  = nullptr;
    QString     error;

    ~ConvergencePj()
    {
        if (pj) {
            proj_destroy(pj);
        }
        if (ctx) {
            proj_context_destroy(ctx);
        }
    }
};

std::shared_ptr<ConvergencePj> makeConvergencePj(const QString& sourceCS)
{
    auto out = std::make_shared<ConvergencePj>();
    out->ctx = cwCoordinateTransformPrivate::createContext();
    if (!out->ctx) {
        out->error = QStringLiteral("Failed to create PROJ context");
        return out;
    }

    PJ* pj = proj_create(out->ctx, sourceCS.toUtf8().constData());
    if (!pj) {
        out->error = QStringLiteral("PROJ failed to create CRS for '%1'").arg(sourceCS);
        return out;
    }

    // PROJ 9.3.0+ proj_factors() handles axis-order internally, but
    // normalize_for_visualization is still required for stable behavior
    // across PROJ point releases — same hardening survex does at
    // datain.c:3709-3724.
    PJ* pj_norm = proj_normalize_for_visualization(out->ctx, pj);
    if (pj_norm) {
        proj_destroy(pj);
        pj = pj_norm;
    }

    out->pj = pj;
    return out;
}

std::shared_ptr<ConvergencePj> cachedConvergencePj(const QString& sourceCS)
{
    thread_local QHash<QString, std::shared_ptr<ConvergencePj>> cache;
    auto it = cache.constFind(sourceCS);
    if (it == cache.constEnd()) {
        if (cache.size() >= 256) {
            cache.clear();
        }
        it = cache.insert(sourceCS, makeConvergencePj(sourceCS));
    }
    return *it;
}

} // namespace

namespace cwGridConvergence {

Monad::Result<double> computeAt(const cwGeoPoint& location,
                                const QString& sourceCS)
{
    const QString normalized = sourceCS.trimmed();
    if (normalized.isEmpty()) {
        return Monad::Result<double>(
            QStringLiteral("Source coordinate system is empty"));
    }

    // Geographic CRS has no grid → no convergence.
    if (cwCoordinateTransform::isGeographic(normalized)) {
        return Monad::Result<double>(0.0);
    }

    // proj_factors wants lon/lat in radians. The caller hands us a point
    // in sourceCS; convert it to WGS84 (degrees) first, then to radians.
    cwCoordinateTransform toWgs84(normalized, cwCoordinateTransform::Wgs84);
    if (!toWgs84.isValid()) {
        return Monad::Result<double>(
            QStringLiteral("Failed to transform from '%1' to WGS84: %2")
                .arg(normalized, toWgs84.errorMessage()));
    }
    const cwGeoPoint geo = toWgs84.transform(location);

    auto pjCtx = cachedConvergencePj(normalized);
    if (!pjCtx->pj) {
        return Monad::Result<double>(pjCtx->error.isEmpty()
            ? QStringLiteral("Failed to build PROJ convergence context")
            : pjCtx->error);
    }

    PJ_COORD lp;
    lp.lp.lam = qDegreesToRadians(geo.x);
    lp.lp.phi = qDegreesToRadians(geo.y);
    const PJ_FACTORS factors = proj_factors(pjCtx->pj, lp);

    return Monad::Result<double>(qRadiansToDegrees(factors.meridian_convergence));
}

} // namespace cwGridConvergence
