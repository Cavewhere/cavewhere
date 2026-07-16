/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwMeasurementLineRenderer.h"

#include "cwCamera.h"
#include "cwMeasurementInteraction.h"
#include "cwMeasurementLineCanvas.h"
#include "cwScene.h"

//Qt includes
#include <QCanvasPainter>

namespace {
    constexpr float kMeasurementLineWidth = 2.0f;
    constexpr float kRubberBandWidth = 1.5f;
    constexpr int kRubberBandAlpha = 180;
}

cwMeasurementLineRenderer::cwMeasurementLineRenderer() = default;

cwMeasurementLineRenderer::~cwMeasurementLineRenderer() = default;

void cwMeasurementLineRenderer::synchronize(QCanvasPainterItem* item)
{
    auto* canvas = static_cast<cwMeasurementLineCanvas*>(item);

    m_hasFirst = false;
    m_hasMeasurement = false;
    m_endValid = false;
    m_lineColor = canvas->lineColor();
    m_invalidColor = canvas->invalidColor();

    cwMeasurementInteraction* interaction = canvas->interaction();
    cwCamera* camera = canvas->camera();
    if (interaction == nullptr || camera == nullptr) {
        return;
    }

    m_hasFirst = interaction->hasFirst();
    m_hasMeasurement = interaction->hasMeasurement();

    // GUI thread is blocked during synchronize — safe to read camera matrices.
    m_firstScreen = camera->project(interaction->firstPoint());

    if (m_hasMeasurement) {
        m_endScreen = camera->project(interaction->secondPoint());
        m_endValid = true;
    } else if (interaction->hoverValid()) {
        // A click here would place a point — anchor the line to the world hit.
        m_endScreen = camera->project(interaction->hoverPoint());
        m_endValid = true;
    } else {
        // The ray missed (or Station-only over empty space): the line still
        // follows the cursor so it stays attached, but in the invalid color.
        m_endScreen = interaction->hoverScreenPoint();
        m_endValid = false;
    }
}

void cwMeasurementLineRenderer::paint(QCanvasPainter* painter)
{
    if (!m_hasFirst) {
        return;
    }

    painter->setRenderHint(QCanvasPainter::RenderHint::Antialiasing);
    painter->setLineCap(QCanvasPainter::LineCap::Round);

    if (m_hasMeasurement) {
        painter->setStrokeStyle(m_lineColor);
        painter->setLineWidth(kMeasurementLineWidth);
        painter->beginPath();
        painter->moveTo(m_firstScreen);
        painter->lineTo(m_endScreen);
        painter->stroke();
        return;
    }

    // Awaiting the second point. A valid hover gets the faded accent rubber-band;
    // an invalid one (no pickable geometry under the cursor) gets the invalid
    // color at full strength, signalling that a click won't measure anything.
    QColor stroke;
    if (m_endValid) {
        stroke = m_lineColor;
        stroke.setAlpha(kRubberBandAlpha);
    } else {
        stroke = m_invalidColor;
    }
    painter->setStrokeStyle(stroke);
    painter->setLineWidth(kRubberBandWidth);
    painter->beginPath();
    painter->moveTo(m_firstScreen);
    painter->lineTo(m_endScreen);
    painter->stroke();
}
