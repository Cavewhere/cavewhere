/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwProgressRingRenderer.h"
#include "cwProgressRing.h"

//Qt includes
#include <QCanvasPainter>

//Std includes
#include <algorithm>
#include <numbers>

namespace {
    //Proportions of the item's shorter side, taken from the design's 24px ring
    //with a radius of 9 and a 2.4px stroke, so the mark keeps its weight at the
    //sidebar's 24px and the top bar chip's 14px alike.
    constexpr double kRadiusRatio = 0.375;
    constexpr double kLineWidthRatio = 0.1;

    //Enough of the circle to read as a moving segment rather than a dot
    constexpr double kIndeterminateSweepFraction = 0.3;

    constexpr double kTwoPi = 2.0 * std::numbers::pi;

    //Twelve o'clock, so a determinate arc grows the way a clock hand does
    constexpr double kDeterminateStartAngle = -0.5 * std::numbers::pi;

    constexpr double kDegreesToRadians = std::numbers::pi / 180.0;
}

cwProgressRingRenderer::cwProgressRingRenderer() = default;

cwProgressRingRenderer::~cwProgressRingRenderer() = default;

void cwProgressRingRenderer::synchronizeData(QCanvasPainterItem* item)
{
    auto* ring = static_cast<cwProgressRing*>(item);

    m_progress = ring->progress();
    m_spinAngle = ring->spinAngle();
    m_trackColor = ring->trackColor();
    m_arcColor = ring->arcColor();
}

void cwProgressRingRenderer::paint(QCanvasPainter* painter)
{
    const double size = (std::min)(width(), height());
    if (size <= 0.0) {
        return;
    }

    const QPointF center(width() * 0.5, height() * 0.5);
    const double radius = size * kRadiusRatio;
    const double lineWidth = size * kLineWidthRatio;

    painter->setRenderHint(QCanvasPainter::RenderHint::Antialiasing);
    painter->setLineWidth(static_cast<float>(lineWidth));

    painter->setLineCap(QCanvasPainter::LineCap::Butt);
    painter->setStrokeStyle(m_trackColor);
    painter->beginPath();
    painter->circle(center, static_cast<float>(radius));
    painter->stroke();

    //Zero is not a determinate state to draw: a measured job that has finished
    //no step yet would otherwise be a bare track, which reads as stalled rather
    //than as starting. It spins until there is an arc worth drawing, the same as
    //a job that reports no steps at all.
    const bool determinate = m_progress > 0.0;
    const double sweep = determinate
            ? std::clamp(m_progress, 0.0, 1.0) * kTwoPi
            : kIndeterminateSweepFraction * kTwoPi;

    const double start = determinate
            ? kDeterminateStartAngle
            : kDeterminateStartAngle + m_spinAngle * kDegreesToRadians;

    painter->setLineCap(QCanvasPainter::LineCap::Round);
    painter->setStrokeStyle(m_arcColor);
    painter->beginPath();
    painter->arc(center,
                 static_cast<float>(radius),
                 static_cast<float>(start),
                 static_cast<float>(start + sweep),
                 QCanvasPainter::PathWinding::ClockWise,
                 QCanvasPainter::PathConnection::NotConnected);
    painter->stroke();
}
