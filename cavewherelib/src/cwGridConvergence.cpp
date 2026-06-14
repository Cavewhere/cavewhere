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

    ConvergencePj() = default;

    ~ConvergencePj()
    {
        if (pj) {
            proj_destroy(pj);
        }
        if (ctx) {
            proj_context_destroy(ctx);
        }
    }

    Q_DISABLE_COPY_MOVE(ConvergencePj)
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

cwGridConvergence::cwGridConvergence(QObject* parent) :
    QObject(parent)
{
}

QString cwGridConvergence::text() const
{
    switch (m_state) {
    case Valid:
        return QStringLiteral("%1° at %2").arg(m_angle, 0, 'f', 2).arg(m_station);
    case NoFixStation:
        return QStringLiteral("n/a (no fix station)");
    case NoCoordinateSystem:
        return QStringLiteral("n/a (no coordinate system)");
    case Geographic:
        return QStringLiteral("n/a (geographic CS)");
    case Error:
        return QStringLiteral("n/a (%1)").arg(m_error);
    }
    return QString();
}

QString cwGridConvergence::detailText() const
{
    if (m_state == Valid) {
        return QStringLiteral("%1 (%2)").arg(text(), m_coordinateSystem);
    }
    return text();
}

void cwGridConvergence::update(const QList<cwFixStation>& fixStations,
                               const QString& fallbackCoordinateSystem)
{
    // 0.0 / empty for every non-Valid case — no grid means no rotation.
    struct Resolved { double angle; State state; QString station; QString cs; QString error; };
    const Resolved resolved = [&]() -> Resolved {
        if (fixStations.isEmpty()) {
            return { 0.0, NoFixStation, QString(), QString(), QString() };
        }

        const cwFixStation& first = fixStations.first();
        QString sourceCS = first.inputCS().trimmed();
        if (sourceCS.isEmpty()) {
            sourceCS = fallbackCoordinateSystem.trimmed();
        }

        if (sourceCS.isEmpty()) {
            return { 0.0, NoCoordinateSystem, QString(), QString(), QString() };
        }
        if (cwCoordinateTransform::isGeographic(sourceCS)) {
            return { 0.0, Geographic, QString(), QString(), QString() };
        }

        const cwGeoPoint location(first.easting(), first.northing(), first.elevation());
        const auto convergence = computeAt(location, sourceCS);
        if (convergence.hasError()) {
            return { 0.0, Error, QString(), QString(), convergence.errorMessage() };
        }

        const QString station = first.stationName().isEmpty()
            ? QStringLiteral("fix station")
            : first.stationName();
        const QString csName = cwCoordinateTransform::nameFor(sourceCS);
        const QString csLabel = csName.isEmpty() ? sourceCS : csName;
        return { convergence.value(), Valid, station, csLabel, QString() };
    }();

    if (resolved.angle == m_angle
        && resolved.state == m_state
        && resolved.station == m_station
        && resolved.cs == m_coordinateSystem
        && resolved.error == m_error) {
        return;
    }

    m_angle = resolved.angle;
    m_state = resolved.state;
    m_station = resolved.station;
    m_coordinateSystem = resolved.cs;
    m_error = resolved.error;
    emit changed();
}

Monad::Result<double> cwGridConvergence::computeAt(const cwGeoPoint& location,
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

    // proj_factors() reports failure through the context errno, not its return
    // value — on an out-of-domain point it hands back a zero-filled struct,
    // making a real failure indistinguishable from a legitimate 0.0 (the value
    // on the central meridian). The PJ is cached across calls, so reset first
    // to avoid reading a stale errno from a previous failure.
    proj_errno_reset(pjCtx->pj);
    const PJ_FACTORS factors = proj_factors(pjCtx->pj, lp);
    const int err = proj_errno(pjCtx->pj);
    if (err != 0) {
        return Monad::Result<double>(
            QStringLiteral("PROJ proj_factors failed for '%1': %2")
                .arg(normalized, QString::fromUtf8(proj_errno_string(err))));
    }

    const double convergence = qRadiansToDegrees(factors.meridian_convergence);
    if (!qIsFinite(convergence)) {
        return Monad::Result<double>(
            QStringLiteral("PROJ returned a non-finite convergence for '%1'")
                .arg(normalized));
    }

    return Monad::Result<double>(convergence);
}
