/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwGridConvergence.h"
#include "cwCave.h"
#include "cwCoordinateTransform.h"
#include "cwCoordinateTransformPrivate.h"
#include "cwUnits.h"

//Qt includes
#include <QHash>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <memory>

namespace {

// Per-sourceCS PJ cached on the calling thread. proj_factors() is the
// expensive call (sqlite hit during proj_create plus CRS construction),
// and cwAzimuthReference reads convergence once per measurement — caching
// keeps it effectively free on the second hit. PJ_CONTEXT is per-instance and
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

// Convergence grows at tan(latitude)/EarthRadius per meter of east-west offset
// from the frame's central meridian — 0.0075 degrees per kilometer at 40N. Two
// decimals would round every cave within 1.3 km of the center to "0.00", which
// reads as a feature that isn't working rather than one whose whole point is
// that the angle is small; three resolves 130 m. Six is past anything
// measurable (13 cm), which is what the tooltip is for.
constexpr int kCompactDecimals = 3;
constexpr int kDetailDecimals = 6;

//! True when \a angle has no digits left at \a decimals places — its magnitude
//! sits below half of the last one printed.
bool printsAsZero(double angle, int decimals)
{
    return qAbs(angle) < 0.5 * std::pow(10.0, -decimals);
}

//! The reading both precisions share: the angle, and the fix it was read at.
//! cwUnits::formatAngle drops the sign on a value that prints as zero, which
//! matters here twice over — a cave a hair west of the central meridian would
//! read "-0.000°", and PROJ answers the anchor itself with a tiny negative
//! rather than a true zero.
QString readoutAt(double angle, const QString& station, int decimals)
{
    return QStringLiteral("%1 at %2").arg(cwUnits::formatAngle(angle, decimals), station);
}

} // namespace

cwGridConvergence::cwGridConvergence(QObject* parent) :
    QObject(parent)
{
}

double cwGridConvergence::angleForCave(const cwCave* cave)
{
    return cave != nullptr ? cave->gridConvergence()->angle() : 0.0;
}

QString cwGridConvergence::text() const
{
    switch (m_state) {
    case Valid:
        return readoutAt(m_angle, m_station, kCompactDecimals);
    case NoFixStation:
        return QStringLiteral("n/a (no fix station)");
    case NoCoordinateSystem:
        return QStringLiteral("n/a (no coordinate system)");
    case Error:
        return QStringLiteral("n/a (%1)").arg(m_error);
    }
    return QString();
}

QString cwGridConvergence::detailText() const
{
    if (m_state != Valid) {
        return text();
    }

    // The sentence fires exactly when every digit the tooltip prints is zero —
    // the anchor cave always, any cave on the meridian besides. That is the one
    // reading more precision cannot explain, so prose has to. Every other
    // reading names the grid instead: it is always the same one, and saying so
    // beats repeating the proj string PROJ has no name for.
    const QString grid = printsAsZero(m_angle, kDetailDecimals)
        ? QStringLiteral(" — this cave sits on the local projection's central "
                         "meridian, where grid north and true north point the "
                         "same way.")
        : QStringLiteral(", in the project's local projection");

    return readoutAt(m_angle, m_station, kDetailDecimals) + grid;
}

void cwGridConvergence::update(const QList<cwFixStation>& fixStations,
                               const QString& frameCS)
{
    // 0.0 / empty for every non-Valid case — no grid means no rotation.
    struct Resolved { double angle; State state; QString station; QString error; };
    const Resolved resolved = [&]() -> Resolved {
        if (fixStations.isEmpty()) {
            return { 0.0, NoFixStation, QString(), QString() };
        }

        const QString frame = frameCS.trimmed();
        if (frame.isEmpty()) {
            return { 0.0, NoCoordinateSystem, QString(), QString() };
        }

        // Only a fix that reads as a coordinate under a system of its own says
        // where the cave is; the rest report zeros, which would converge at the
        // frame's origin rather than at the cave.
        const auto located =
            std::find_if(fixStations.constBegin(), fixStations.constEnd(),
                         [](const cwFixStation& fix) {
                             return fix.hasPlacedCoordinate()
                                    && cwCoordinateTransform::isValidCS(fix.inputCS().trimmed());
                         });
        if (located == fixStations.constEnd()) {
            return { 0.0, NoFixStation, QString(), QString() };
        }

        const QString locatedCS = located->inputCS().trimmed();
        const cwGeoPoint location(located->easting(), located->northing(), located->elevation());
        const auto inFrame = cwCoordinateTransform::transformPoint(locatedCS, frame, location);
        if (!inFrame.has_value()) {
            return { 0.0, Error, QString(),
                     QStringLiteral("Failed to place '%1' in the project's frame")
                         .arg(located->stationName()) };
        }

        const auto convergence = computeAt(*inFrame, frame);
        if (convergence.hasError()) {
            return { 0.0, Error, QString(), convergence.errorMessage() };
        }

        const QString station = located->stationName().isEmpty()
            ? QStringLiteral("fix station")
            : located->stationName();
        return { convergence.value(), Valid, station, QString() };
    }();

    if (resolved.angle == m_angle
        && resolved.state == m_state
        && resolved.station == m_station
        && resolved.error == m_error) {
        return;
    }

    m_angle = resolved.angle;
    m_state = resolved.state;
    m_station = resolved.station;
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
    // Through transformPoint so the PJ comes out of its cache: every cave in
    // a project converges under the same frame, so this leg is the same
    // transform once per cave otherwise.
    const auto geo = cwCoordinateTransform::transformPoint(
        normalized, cwCoordinateTransform::Wgs84, location);
    if (!geo.has_value()) {
        return Monad::Result<double>(
            QStringLiteral("Failed to transform from '%1' to WGS84").arg(normalized));
    }

    auto pjCtx = cachedConvergencePj(normalized);
    if (!pjCtx->pj) {
        return Monad::Result<double>(pjCtx->error.isEmpty()
            ? QStringLiteral("Failed to build PROJ convergence context")
            : pjCtx->error);
    }

    PJ_COORD lp;
    lp.lp.lam = qDegreesToRadians(geo->x);
    lp.lp.phi = qDegreesToRadians(geo->y);

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
