/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwMeasurementInteraction.h"

//Our includes
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwMeasurementMath.h"

//Std includes
#include <cmath>

//Qt includes
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QSettings>
#include <QString>

namespace {
    // Clipboard readout mirrors the on-screen popup: metres for lengths, degrees
    // for angles, at the same precision the panel shows.
    constexpr int kLengthDecimals = 2;
    constexpr int kAngleDecimals = 1;

    // Signed metres at display precision for the by-axis components; the sign is
    // the direction (east/west, north/south, up/down). Mirrors the popup's
    // _signedLength: a positive value gets an explicit '+', and a value that
    // rounds to zero shows no sign (a rounded -0 renders a clean "0.00 m").
    QString formatSignedLength(double meters)
    {
        const double scale = std::pow(10.0, kLengthDecimals);
        double rounded = std::round(meters * scale) / scale;
        if (rounded == 0.0) {
            rounded = 0.0; // collapse -0.0 so a rounds-to-zero component shows no sign
        }
        const QString sign = rounded > 0.0 ? QStringLiteral("+") : QString();
        return sign + QStringLiteral("%1 m").arg(rounded, 0, 'f', kLengthDecimals);
    }

    QString azimuthReferenceKey() { return QStringLiteral("measurement/azimuthReference"); }

    // The parenthetical that tags the azimuth in the clipboard readout, so a
    // pasted measurement says which north it is against (matching the on-screen
    // selector). Magnetic carries "today" because it's the present-day field.
    QString azimuthReferenceClipboardLabel(cwAzimuthReference::Reference reference)
    {
        switch (reference) {
        case cwAzimuthReference::Reference::True:     return QStringLiteral("true");
        case cwAzimuthReference::Reference::Magnetic: return QStringLiteral("magnetic, today");
        case cwAzimuthReference::Reference::Grid:     break;
        }
        return QStringLiteral("grid");
    }

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
    // construction. With no measurement yet this takes the cheap grid-passthrough
    // path; the detailed resolve waits for a frozen, expanded readout.
    refreshReference();
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

cwGeoReference* cwMeasurementInteraction::geoReference() const
{
    return m_geoReference;
}

bool cwMeasurementInteraction::geoReferenced() const
{
    return m_geoReference && m_geoReference->hasCoordinateSystem();
}

void cwMeasurementInteraction::setGeoReference(cwGeoReference* geoReference)
{
    if (m_geoReference == geoReference) {
        return;
    }
    // The geo-reference supplies the worldOrigin and CRS that turn the grid
    // azimuth into a true/magnetic bearing. A CRS change can also invalidate the
    // current reference (a local-only project has none), so it routes through
    // syncReferenceToGeoReference; an origin change only moves the location.
    if (m_geoReference) {
        disconnect(m_geoReference, &cwGeoReference::globalCoordinateSystemChanged,
                   this, &cwMeasurementInteraction::syncReferenceToGeoReference);
        disconnect(m_geoReference, &cwGeoReference::worldOriginChanged,
                   this, &cwMeasurementInteraction::refreshReference);
    }
    m_geoReference = geoReference;
    if (m_geoReference) {
        connect(m_geoReference, &cwGeoReference::globalCoordinateSystemChanged,
                this, &cwMeasurementInteraction::syncReferenceToGeoReference);
        connect(m_geoReference, &cwGeoReference::worldOriginChanged,
                this, &cwMeasurementInteraction::refreshReference);
    }
    emit geoReferenceChanged();
    syncReferenceToGeoReference();
}

void cwMeasurementInteraction::setCalculateDetails(bool calculateDetails)
{
    if (m_calculateDetails == calculateDetails) {
        return;
    }
    m_calculateDetails = calculateDetails;
    emit calculateDetailsChanged();
    // Turning this on for a frozen measurement is the trigger that resolves the
    // detailed reference; turning it off drops back to the cheap grid bearing.
    refreshReference();
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
    refreshReference();
}

void cwMeasurementInteraction::syncReferenceToGeoReference()
{
    // A geo-reference/CRS change can leave True or Magnetic selected on a project
    // that no longer supports them; snap the active selection back to Grid
    // (persisted, per the chosen behavior). When the selection is still valid we
    // just re-resolve for the new origin and CRS.
    if (!geoReferenced() && m_azimuthReference != cwAzimuthReference::Reference::Grid) {
        setAzimuthReference(cwAzimuthReference::Reference::Grid);
    } else {
        refreshReference();
    }
}

void cwMeasurementInteraction::refreshReference()
{
    using Reference = cwAzimuthReference::Reference;

    // The True/Magnetic resolve rebuilds a PROJ transform (grid convergence) and
    // runs IGRF (declination) — too costly to repeat per hover. Run it only for a
    // frozen measurement whose detailed readout is on screen; otherwise (live
    // preview, collapsed chip, hidden panel, or the always-grid default) the
    // azimuth row tracks the cheap grid bearing with no coordinate transform.
    const bool resolveDetailed = m_hasMeasurement
                                 && m_calculateDetails
                                 && m_azimuthReference != Reference::Grid;
    if (!resolveDetailed) {
        applyReferenceResult(cwAzimuthReference::resolve(
                m_azimuth, Reference::Grid, cwGeoPoint{}, QString{}, QDateTime{}));
        return;
    }

    // Resolve against the first picked point (per spec). UTC is enough for IGRF
    // (it keys on the decimal year) and is faster and DST-stable versus local.
    const cwGeoPoint location = m_geoReference ? m_geoReference->toGlobal(m_firstPoint)
                                               : cwGeoPoint{};
    const QString sourceCS = m_geoReference ? m_geoReference->globalCoordinateSystem()
                                            : QString{};
    applyReferenceResult(cwAzimuthReference::resolve(
                m_azimuth, m_azimuthReference, location, sourceCS,
                QDateTime::currentDateTimeUtc()));
}

void cwMeasurementInteraction::applyReferenceResult(const cwAzimuthReference::Result& result)
{
    if (m_referenceAzimuth == result.azimuth
            && m_referenceAvailable == result.available
            && m_convergence == result.convergence
            && m_declination == result.declination
            && m_referenceReason == result.reason) {
        return;
    }
    m_referenceAzimuth = result.azimuth;
    m_referenceAvailable = result.available;
    m_referenceReason = result.reason;
    m_convergence = result.convergence;
    m_declination = result.declination;
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
    refreshReference();
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
    refreshReference();
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
    // Freeze before updating so refreshReference() sees a complete measurement
    // and resolves the detailed reference when the panel is already expanded.
    m_hasMeasurement = true;
    updateMeasurement(m_firstPoint, m_secondPoint);
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

    // The azimuth is reported against the selected reference so the pasted text
    // matches the on-screen readout. An unavailable reference (no/invalid CRS)
    // pastes "n/a (reason)" rather than a silently-wrong grid value. Grid always
    // resolves, so m_referenceAzimuth is the grid azimuth in that case.
    const QString azimuthValue = m_referenceAvailable
            ? QStringLiteral("%1°").arg(m_referenceAzimuth, 0, 'f', kAngleDecimals)
            : QStringLiteral("n/a (%1)").arg(m_referenceReason);
    const QString azimuthLine =
            QStringLiteral("Azimuth (%1): %2")
            .arg(azimuthReferenceClipboardLabel(m_azimuthReference), azimuthValue);

    // Mirrors the on-screen readout (MeasurementReadoutPopup): the same grouped
    // labels, the same signed by-axis components, and the same precision, so a
    // pasted measurement reads exactly like the panel. azimuthLine is
    // concatenated, not fed through a numbered placeholder: it can carry
    // referenceReason (a PROJ/IGRF error string) whose embedded %N would be
    // re-targeted by the later .arg() calls. Vertical is the ΔZ component, so it
    // is reported once, under By Axis (no separate ΔUp).
    const QString text =
            QStringLiteral("Distance\n"
                           "  Straight-line (3D): %1 m\n"
                           "  Horizontal (2D): %2 m\n"
                           "Direction\n  ")
              .arg(m_distance, 0, 'f', kLengthDecimals)
              .arg(m_horizontal, 0, 'f', kLengthDecimals)
            + azimuthLine
            + QStringLiteral("\n"
                             "  Inclination: %1°\n"
                             "By Axis\n"
                             "  Easting (X): %2\n"
                             "  Northing (Y): %3\n"
                             "  Vertical (Z): %4")
              .arg(m_inclination, 0, 'f', kAngleDecimals)
              .arg(formatSignedLength(m_deltaEast),
                   formatSignedLength(m_deltaNorth),
                   formatSignedLength(m_vertical));

    if (QClipboard* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}
