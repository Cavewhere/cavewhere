/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwMeasurementLineCanvas.h"

#include "cwCamera.h"
#include "cwMeasurementInteraction.h"
#include "cwMeasurementLineRenderer.h"
#include "cwScene.h"

cwMeasurementLineCanvas::cwMeasurementLineCanvas(QQuickItem* parent) :
    QCanvasPainterItem(parent)
{
}

cwMeasurementLineCanvas::~cwMeasurementLineCanvas() = default;

void cwMeasurementLineCanvas::setInteraction(cwMeasurementInteraction* interaction)
{
    if (m_interaction == interaction) {
        return;
    }
    if (!m_interaction.isNull()) {
        disconnect(m_interaction, nullptr, this, nullptr);
    }
    m_interaction = interaction;
    connectInteraction(interaction);
    emit interactionChanged();
    update();
}

void cwMeasurementLineCanvas::setCamera(cwCamera* camera)
{
    if (m_camera == camera) {
        return;
    }
    if (!m_camera.isNull()) {
        disconnect(m_camera, nullptr, this, nullptr);
    }
    m_camera = camera;
    connectCamera(camera);
    emit cameraChanged();
    update();
}

void cwMeasurementLineCanvas::setLineColor(QColor color)
{
    if (m_lineColor == color) {
        return;
    }
    m_lineColor = color;
    emit lineColorChanged();
    update();
}

void cwMeasurementLineCanvas::setInvalidColor(QColor color)
{
    if (m_invalidColor == color) {
        return;
    }
    m_invalidColor = color;
    emit invalidColorChanged();
    update();
}

QCanvasPainterItemRenderer* cwMeasurementLineCanvas::createItemRenderer() const
{
    return new cwMeasurementLineRenderer();
}

void cwMeasurementLineCanvas::connectInteraction(cwMeasurementInteraction* interaction)
{
    if (interaction == nullptr) {
        return;
    }
    connect(interaction, &cwMeasurementInteraction::measurementChanged,
            this, [this]() { update(); });
    connect(interaction, &cwMeasurementInteraction::hoverChanged,
            this, [this]() { update(); });
}

void cwMeasurementLineCanvas::connectCamera(cwCamera* camera)
{
    if (camera == nullptr) {
        return;
    }
    // Re-project the segment whenever the camera pose or viewport changes so
    // the line tracks the geometry while the user orbits or zooms.
    connect(camera, &cwCamera::viewMatrixChanged, this, [this]() { update(); });
    connect(camera, &cwCamera::projectionChanged, this, [this]() { update(); });
    connect(camera, &cwCamera::viewportChanged, this, [this]() { update(); });
}
