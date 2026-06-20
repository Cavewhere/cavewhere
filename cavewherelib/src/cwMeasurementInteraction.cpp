/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwMeasurementInteraction.h"

//Our includes
#include "cwCamera.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"
#include "cwMeasurementMath.h"
#include "cwScenePick.h"

//Qt includes
#include <QClipboard>
#include <QGuiApplication>
#include <QString>

namespace {
    // Screen-space snap tolerance, in millimeters, matching cwCoordinatePicker.
    // Millimeters (not pixels) so the snap covers the same physical size on
    // screen regardless of display DPI or scaling.
    constexpr double kPickRadiusMillimeters = 1.5;

    // Clipboard readout is canonical SI: metres for lengths, degrees for angles.
    constexpr int kLengthDecimals = 3;
    constexpr int kAngleDecimals = 1;
}

cwMeasurementInteraction::cwMeasurementInteraction(QQuickItem* parent) :
    cwInteraction(parent)
{
}

cwMeasurementInteraction::~cwMeasurementInteraction() = default;

void cwMeasurementInteraction::setCamera(cwCamera* camera)
{
    if (m_camera == camera) {
        return;
    }
    m_camera = camera;
    emit cameraChanged();
}

void cwMeasurementInteraction::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }
    m_scene = scene;
    emit sceneChanged();
}

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

cwMeasurementInteraction::Pick cwMeasurementInteraction::pickAt(QPointF screenPoint) const
{
    Pick pick;
    if (!m_camera || !m_scene) {
        return pick;
    }
    cwGeometryItersecter* intersecter = m_scene->geometryItersecter();
    if (!intersecter) {
        return pick;
    }

    const cwScenePick::Result result = cwScenePick::snappedPoint(
                screenPoint, *m_camera, *intersecter, pixelsForMillimeters(kPickRadiusMillimeters));
    pick.world = result.world;
    pick.hit = result.hit;
    pick.snapped = result.snappedToStation;
    return pick;
}

bool cwMeasurementInteraction::isPlaceable(const Pick& pick) const
{
    return pick.hit && (m_mode == Mode::Free || pick.snapped);
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
}

void cwMeasurementInteraction::hover(QPointF screenPoint)
{
    const Pick pick = pickAt(screenPoint);

    m_hoverScreenPoint = screenPoint;
    m_hoverSnapped = pick.hit && pick.snapped;
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
