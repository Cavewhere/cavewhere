/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazClipPolygonCanvas.h"

#include "cwLazClipInteraction.h"
#include "cwLazClipPolygonRenderer.h"

cwLazClipPolygonCanvas::cwLazClipPolygonCanvas(QQuickItem* parent) :
    QCanvasPainterItem(parent)
{
}

cwLazClipPolygonCanvas::~cwLazClipPolygonCanvas() = default;

void cwLazClipPolygonCanvas::setInteraction(cwLazClipInteraction* interaction)
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

void cwLazClipPolygonCanvas::setHoverScreenPos(QPointF p)
{
    if (m_hoverScreenPos == p) {
        return;
    }
    m_hoverScreenPos = p;
    emit hoverScreenPosChanged();
    update();
}

void cwLazClipPolygonCanvas::setSnapActive(bool active)
{
    if (m_snapActive == active) {
        return;
    }
    m_snapActive = active;
    emit snapActiveChanged();
    update();
}

void cwLazClipPolygonCanvas::setPolygonOutlineColor(QColor c)
{
    if (m_outlineColor == c) {
        return;
    }
    m_outlineColor = c;
    emit polygonOutlineColorChanged();
    update();
}

void cwLazClipPolygonCanvas::setPolygonFillColor(QColor c)
{
    if (m_fillColor == c) {
        return;
    }
    m_fillColor = c;
    emit polygonFillColorChanged();
    update();
}

void cwLazClipPolygonCanvas::setPolygonVertexBorderColor(QColor c)
{
    if (m_vertexBorderColor == c) {
        return;
    }
    m_vertexBorderColor = c;
    emit polygonVertexBorderColorChanged();
    update();
}

QCanvasPainterItemRenderer* cwLazClipPolygonCanvas::createItemRenderer() const
{
    return new cwLazClipPolygonRenderer();
}

void cwLazClipPolygonCanvas::onInteractionPolygonChanged()
{
    update();
}

void cwLazClipPolygonCanvas::onInteractionStateChanged()
{
    update();
}

void cwLazClipPolygonCanvas::connectInteraction(cwLazClipInteraction* interaction)
{
    if (interaction == nullptr) {
        return;
    }
    connect(interaction, &cwLazClipInteraction::polygonChanged,
            this, &cwLazClipPolygonCanvas::onInteractionPolygonChanged);
    connect(interaction, &cwLazClipInteraction::stateChanged,
            this, &cwLazClipPolygonCanvas::onInteractionStateChanged);
}
