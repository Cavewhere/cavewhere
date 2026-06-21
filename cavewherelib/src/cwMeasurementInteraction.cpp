/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwMeasurementInteraction.h"

//Our includes
#include "cwCavingRegion.h"
#include "cwGeoPoint.h"
#include "cwMeasurementMath.h"

//Qt includes
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QSettings>
#include <QString>

namespace {
    // Clipboard readout is canonical SI: metres for lengths, degrees for angles.
    constexpr int kLengthDecimals = 3;
    constexpr int kAngleDecimals = 1;

    QString azimuthReferenceKey() { return QStringLiteral("measurement/azimuthReference"); }

    // Persisted as an int; guard against an out-of-range value from a future or
    // corrupt settings file by falling back to Grid.
    cwAzimuthReference::Reference loadAzimuthReference()
    {
        using Reference = cwAzimuthReference::Reference;
        QSettings settings;
        const int stored = settings.value(azimuthReferenceKey(),
                                           int(Reference::Grid)).toInt();
        switch (stored) {
        case int(Reference::True):     return Reference::True;
        case int(Reference::Magnetic): return Reference::Magnetic;
        default:                       return Reference::Grid;
        }
    }
}

cwMeasurementInteraction::cwMeasurementInteraction(QQuickItem* parent) :
    cwScenePicker(parent),
    m_azimuthReference(loadAzimuthReference())
{
    // Resolve once up front so the reference readout is self-consistent from
    // construction — e.g. a persisted True/Magnetic with no region yet reports
    // n/a rather than the default "available, azimuth 0".
    recomputeReference();
}

cwMeasurementInteraction::~cwMeasurementInteraction() = default;

void cwMeasurementInteraction::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    // Switching to StationOnly mid-measure only gates future placements; an
    // already-placed free point stays put.
    m_mode = mode;
    emit modeChanged();
}

cwCavingRegion* cwMeasurementInteraction::region() const
{
    return m_region;
}

bool cwMeasurementInteraction::geoReferenced() const
{
    return m_region && m_region->hasCoordinateSystem();
}

void cwMeasurementInteraction::setRegion(cwCavingRegion* region)
{
    if (m_region == region) {
        return;
    }
    // The region supplies the worldOrigin and CRS that turn the grid azimuth
    // into a true/magnetic bearing. A CRS change can also invalidate the current
    // reference (a local-only project has none), so it routes through
    // syncReferenceToRegion; an origin change only moves the location. Unlike
    // cwCoordinatePicker we hold no WGS84 transform to rebuild and no pick to
    // clear, so this stays off the cwScenePicker base.
    if (m_region) {
        disconnect(m_region, &cwCavingRegion::globalCoordinateSystemChanged,
                   this, &cwMeasurementInteraction::syncReferenceToRegion);
        disconnect(m_region, &cwCavingRegion::worldOriginChanged,
                   this, &cwMeasurementInteraction::recomputeReference);
    }
    m_region = region;
    if (m_region) {
        connect(m_region, &cwCavingRegion::globalCoordinateSystemChanged,
                this, &cwMeasurementInteraction::syncReferenceToRegion);
        connect(m_region, &cwCavingRegion::worldOriginChanged,
                this, &cwMeasurementInteraction::recomputeReference);
    }
    emit regionChanged();
    syncReferenceToRegion();
}

void cwMeasurementInteraction::setAzimuthReference(cwAzimuthReference::Reference reference)
{
    // True/Magnetic need a coordinate system; without one a local-only project
    // can only report grid north, so coerce the selection to Grid.
    if (reference != cwAzimuthReference::Reference::Grid && !geoReferenced()) {
        reference = cwAzimuthReference::Reference::Grid;
    }
    if (m_azimuthReference == reference) {
        return;
    }
    m_azimuthReference = reference;
    QSettings settings;
    settings.setValue(azimuthReferenceKey(), int(reference));
    emit azimuthReferenceChanged();
    recomputeReference();
}

void cwMeasurementInteraction::syncReferenceToRegion()
{
    // A region/CRS change can leave True or Magnetic selected on a project that
    // no longer supports them; snap the active selection back to Grid (persisted,
    // per the chosen behavior). When the selection is still valid we just
    // re-resolve for the new region's origin and CRS.
    if (!geoReferenced() && m_azimuthReference != cwAzimuthReference::Reference::Grid) {
        setAzimuthReference(cwAzimuthReference::Reference::Grid);
    } else {
        recomputeReference();
    }
}

void cwMeasurementInteraction::recomputeReference()
{
    // Resolve against the first picked point (per spec); during a live preview
    // that point is already placed, so the bearing tracks the moving second end.
    cwGeoPoint location;
    QString sourceCS;
    if (m_region) {
        location = cwGeoPoint::fromSceneLocal(m_firstPoint, m_region->worldOrigin());
        sourceCS = m_region->globalCoordinateSystem();
    }

    // UTC is enough for IGRF (it keys on the decimal year) and is faster and
    // DST-stable versus currentDateTime().
    const cwAzimuthReference::Result resolved = cwAzimuthReference::resolve(
                m_azimuth, m_azimuthReference, location, sourceCS,
                QDateTime::currentDateTimeUtc());

    m_referenceAzimuth = resolved.azimuth;
    m_referenceAvailable = resolved.available;
    m_referenceReason = resolved.reason;
    m_convergence = resolved.convergence;
    m_declination = resolved.declination;
    emit referenceChanged();
}

bool cwMeasurementInteraction::isPlaceable(const cwScenePick::Result& pick) const
{
    return pick.hit && (m_mode == Mode::Free || pick.snappedToStation);
}

void cwMeasurementInteraction::updateMeasurement(QVector3D from, QVector3D to)
{
    const cwMeasurementMath::Measurement m = cwMeasurementMath::between(from, to);
    m_distance = m.distance;
    m_azimuth = m.azimuth;
    m_inclination = m.inclination;
    m_horizontal = m.horizontal;
    m_vertical = m.vertical;
    m_deltaEast = m.deltaEast;
    m_deltaNorth = m.deltaNorth;
    recomputeReference();
}

void cwMeasurementInteraction::clearMeasurementValues()
{
    m_distance = 0.0;
    m_azimuth = 0.0;
    m_inclination = 0.0;
    m_horizontal = 0.0;
    m_vertical = 0.0;
    m_deltaEast = 0.0;
    m_deltaNorth = 0.0;
    recomputeReference();
}

void cwMeasurementInteraction::hover(QPointF screenPoint)
{
    const cwScenePick::Result pick = snapPick(screenPoint);

    m_hoverScreenPoint = screenPoint;
    m_hoverSnapped = pick.hit && pick.snappedToStation;
    m_hoverValid = isPlaceable(pick);
    if (pick.hit) {
        m_hoverPoint = pick.world;
    }
    emit hoverChanged();

    // While awaiting the second point, track the cursor: preview A -> hoverPoint
    // so the readout updates live as the user moves.
    if (m_hasFirst && !m_hasMeasurement && pick.hit) {
        updateMeasurement(m_firstPoint, m_hoverPoint);
        emit measurementChanged();
    }
}

void cwMeasurementInteraction::place(QPointF screenPoint)
{
    // Re-evaluate hover at the exact click location first, then commit that
    // point. The placed point is therefore identical to what the live preview
    // showed, even when the last hover event lagged the click (e.g. a trackpad
    // tap with no preceding move at that pixel).
    hover(screenPoint);
    if (!m_hoverValid) {
        // A miss, or a Station-only click over empty space, is a no-op.
        return;
    }

    // A click once the measurement is complete restarts a fresh measurement.
    if (m_hasMeasurement) {
        reset();
    }

    if (!m_hasFirst) {
        m_firstPoint = m_hoverPoint;
        m_hasFirst = true;
        clearMeasurementValues();
        emit measurementChanged();
        return;
    }

    m_secondPoint = m_hoverPoint;
    updateMeasurement(m_firstPoint, m_secondPoint);
    m_hasMeasurement = true;
    emit measurementChanged();
}

void cwMeasurementInteraction::reset()
{
    if (!m_hasFirst && !m_hasMeasurement) {
        return;
    }
    m_firstPoint = QVector3D();
    m_secondPoint = QVector3D();
    m_hasFirst = false;
    m_hasMeasurement = false;
    clearMeasurementValues();
    emit measurementChanged();
}

void cwMeasurementInteraction::copyToClipboard() const
{
    if (!m_hasMeasurement) {
        return;
    }

    // Vertical is the ΔZ component, so it is reported once (no separate ΔUp).
    const QString text =
            QStringLiteral("Distance: %1 m\n"
                           "Azimuth (grid): %2°\n"
                           "Inclination: %3°\n"
                           "Horizontal: %4 m\n"
                           "Vertical: %5 m\n"
                           "ΔEast: %6 m\n"
                           "ΔNorth: %7 m")
            .arg(m_distance, 0, 'f', kLengthDecimals)
            .arg(m_azimuth, 0, 'f', kAngleDecimals)
            .arg(m_inclination, 0, 'f', kAngleDecimals)
            .arg(m_horizontal, 0, 'f', kLengthDecimals)
            .arg(m_vertical, 0, 'f', kLengthDecimals)
            .arg(m_deltaEast, 0, 'f', kLengthDecimals)
            .arg(m_deltaNorth, 0, 'f', kLengthDecimals);

    if (QClipboard* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}
