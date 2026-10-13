/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwBusyRingRenderer.h"
#include "cwBusyRing.h"

//Qt includes
#include <QCanvasConicalGradient>
#include <QCanvasPainter>

//Std includes
#include <algorithm>
#include <numbers>

namespace {
    //Keeps the ring legible at the 16 px size the app also uses
    constexpr double kMinimumLineWidth = 2.0;

    //The open 30 degrees is where the sweep is transparent, so the tail fades
    //out rather than ending in a cap
    constexpr double kSweepDegrees = 330.0;

    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    constexpr double kDegreesToRadians = std::numbers::pi / 180.0;

    //Twelve o'clock, where the leading edge sits before the item is rotated
    constexpr double kLeadAngle = -0.5 * std::numbers::pi;

    //Positions along the sweep, clockwise from the leading edge, matching the
    //prototype's conic gradient
    constexpr float kTransparentEnd = 0.08f;
    constexpr float kStartPosition = 0.30f;
    constexpr float kMidPosition = 0.70f;
    constexpr float kEndPosition = 0.90f;
    constexpr float kLeadPosition = 1.0f;
}

cwBusyRingRenderer::cwBusyRingRenderer() = default;

cwBusyRingRenderer::~cwBusyRingRenderer() = default;

void cwBusyRingRenderer::synchronizeData(QCanvasPainterItem* item)
{
    auto* ring = static_cast<cwBusyRing*>(item);

    m_startColor = ring->startColor();
    m_midColor = ring->midColor();
    m_endColor = ring->endColor();
    m_leadColor = ring->leadColor();
    m_lineWidthRatio = ring->lineWidthRatio();
}

void cwBusyRingRenderer::paint(QCanvasPainter* painter)
{
    const double size = (std::min)(width(), height());
    const double lineWidth = (std::max)(kMinimumLineWidth, size * m_lineWidthRatio);
    const double radius = (size - lineWidth) * 0.5;
    if (radius <= 0.0) {
        return;
    }

    const QPointF center(width() * 0.5, height() * 0.5);

    //The start color with no alpha rather than transparent black, so the fade
    //into the tail keeps its hue instead of passing through gray
    QColor tailColor = m_startColor;
    tailColor.setAlphaF(0.0f);

    QCanvasConicalGradient sweep(center, static_cast<float>(kLeadAngle));
    sweep.setStops({
        {0.0f, tailColor},
        {kTransparentEnd, tailColor},
        {kStartPosition, m_startColor},
        {kMidPosition, m_midColor},
        {kEndPosition, m_endColor},
        {kLeadPosition, m_leadColor}
    });

    //The round cap at the head reaches exactly to the leading edge, where the
    //gradient wraps back to transparent
    const double capAngle = (lineWidth * 0.5) / radius;
    const double arcEnd = kLeadAngle + kTwoPi - capAngle;
    const double arcStart = arcEnd - kSweepDegrees * kDegreesToRadians;

    painter->setRenderHint(QCanvasPainter::RenderHint::Antialiasing);
    painter->setLineWidth(static_cast<float>(lineWidth));
    painter->setLineCap(QCanvasPainter::LineCap::Round);
    painter->setStrokeStyle(sweep);
    painter->beginPath();
    painter->arc(center,
                 static_cast<float>(radius),
                 static_cast<float>(arcStart),
                 static_cast<float>(arcEnd),
                 QCanvasPainter::PathWinding::ClockWise,
                 QCanvasPainter::PathConnection::NotConnected);
    painter->stroke();
}
