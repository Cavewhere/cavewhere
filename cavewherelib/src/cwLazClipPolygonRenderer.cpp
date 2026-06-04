/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazClipPolygonRenderer.h"

#include "cwLazClipPolygonCanvas.h"

//Qt includes
#include <QCanvasPainter>
#include <QList>
#include <QPointF>
#include <QVector3D>

namespace {
constexpr float kClosedOutlineWidth = 2.0f;
constexpr float kDrawingOutlineWidth = 1.5f;
constexpr float kRubberBandWidth = 1.0f;
constexpr float kVertexRadius = 4.0f;
constexpr float kVertexBorderWidth = 1.0f;
constexpr float kSnapHaloRadius = 9.0f;
constexpr int kInProgressOutlineAlpha = 180;
constexpr int kSnapHaloAlpha = 130;

void traceOpenPolyline(QCanvasPainter* p, const QPolygonF& poly)
{
    if (poly.isEmpty()) {
        return;
    }
    p->beginPath();
    p->moveTo(poly.first());
    for (int i = 1; i < poly.size(); ++i) {
        p->lineTo(poly.at(i));
    }
}

void traceClosedPolygon(QCanvasPainter* p, const QPolygonF& poly)
{
    traceOpenPolyline(p, poly);
    if (!poly.isEmpty()) {
        p->closePath();
    }
}
} // namespace

cwLazClipPolygonRenderer::cwLazClipPolygonRenderer() = default;

cwLazClipPolygonRenderer::~cwLazClipPolygonRenderer() = default;

void cwLazClipPolygonRenderer::synchronize(QCanvasPainterItem* item)
{
    auto* canvas = static_cast<cwLazClipPolygonCanvas*>(item);

    m_polygonScreen.clear();
    m_hoverScreenPos = canvas->hoverScreenPos();
    m_snapActive = canvas->snapActive();
    m_outlineColor = canvas->polygonOutlineColor();
    m_fillColor = canvas->polygonFillColor();
    m_vertexBorderColor = canvas->polygonVertexBorderColor();
    m_state = cwLazClipInteraction::State::Idle;

    cwLazClipInteraction* interaction = canvas->interaction();
    if (interaction == nullptr) {
        return;
    }
    m_state = interaction->state();

    // GUI thread is blocked during synchronize — safe to project each
    // world vertex through the camera here.
    const QList<QVector3D>& worldPoints = interaction->polygonWorldXYZ();
    m_polygonScreen.reserve(worldPoints.size());
    for (const QVector3D& w : worldPoints) {
        m_polygonScreen.append(interaction->worldToScreen(w));
    }
}

void cwLazClipPolygonRenderer::paint(QCanvasPainter* painter)
{
    if (m_polygonScreen.isEmpty()) {
        return;
    }
    painter->setRenderHint(QCanvasPainter::RenderHint::Antialiasing);

    const bool closed = (m_state == cwLazClipInteraction::State::Closed
                          || m_state == cwLazClipInteraction::State::Processing);

    QColor inProgressOutline = m_outlineColor;
    inProgressOutline.setAlpha(kInProgressOutlineAlpha);

    // Filled interior — only meaningful once the polygon is closed.
    if (closed && m_polygonScreen.size() >= 3) {
        traceClosedPolygon(painter, m_polygonScreen);
        painter->setFillStyle(m_fillColor);
        painter->fill();
    }

    // Outline (closed) or polyline (still drawing).
    painter->setStrokeStyle(closed ? m_outlineColor : inProgressOutline);
    painter->setLineWidth(closed ? kClosedOutlineWidth : kDrawingOutlineWidth);
    painter->setLineCap(QCanvasPainter::LineCap::Round);
    painter->setLineJoin(QCanvasPainter::LineJoin::Round);
    if (closed) {
        traceClosedPolygon(painter, m_polygonScreen);
        painter->stroke();
    } else {
        traceOpenPolyline(painter, m_polygonScreen);
        painter->stroke();

        // Rubber-band segment from last vertex to current hover. No dashed
        // pen — QCanvasPainter doesn't support that. Distinguished from the
        // committed polyline by the faded color we already set.
        if (m_state == cwLazClipInteraction::State::Drawing
            && !m_polygonScreen.isEmpty()
            && !m_hoverScreenPos.isNull()) {
            painter->setLineWidth(kRubberBandWidth);
            painter->beginPath();
            painter->moveTo(m_polygonScreen.last());
            painter->lineTo(m_hoverScreenPos);
            painter->stroke();
        }
    }

    // Vertex handles — drawn last so they sit on top of fill and outline.
    painter->setFillStyle(m_outlineColor);
    painter->setStrokeStyle(m_vertexBorderColor);
    painter->setLineWidth(kVertexBorderWidth);
    for (const QPointF& p : m_polygonScreen) {
        painter->beginPath();
        painter->circle(p, kVertexRadius);
        painter->fill();
        painter->beginPath();
        painter->circle(p, kVertexRadius);
        painter->stroke();
    }

    // Snap halo around the first vertex when the cursor is in range.
    if (m_snapActive && !m_polygonScreen.isEmpty()) {
        QColor halo = m_outlineColor;
        halo.setAlpha(kSnapHaloAlpha);
        painter->setFillStyle(halo);
        painter->beginPath();
        painter->circle(m_polygonScreen.first(), kSnapHaloRadius);
        painter->fill();
    }
}
