/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwBusyRing.h"
#include "cwBusyRingRenderer.h"

namespace {
    //About 3 px at the style's 44 px indicator, the weight of the prototype's ring
    constexpr double kDefaultLineWidthRatio = 0.07;
}

cwBusyRing::cwBusyRing(QQuickItem* parent) :
    QCanvasPainterItem(parent),
    m_lineWidthRatio(kDefaultLineWidthRatio)
{
    //QCanvasPainterItem fills its rect with opaque black and composites
    //opaquely by default; the ring is drawn over whatever is behind it.
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
}

cwBusyRing::~cwBusyRing() = default;

void cwBusyRing::setStartColor(QColor color)
{
    if (m_startColor == color) {
        return;
    }
    m_startColor = color;
    emit startColorChanged();
    update();
}

void cwBusyRing::setMidColor(QColor color)
{
    if (m_midColor == color) {
        return;
    }
    m_midColor = color;
    emit midColorChanged();
    update();
}

void cwBusyRing::setEndColor(QColor color)
{
    if (m_endColor == color) {
        return;
    }
    m_endColor = color;
    emit endColorChanged();
    update();
}

void cwBusyRing::setLeadColor(QColor color)
{
    if (m_leadColor == color) {
        return;
    }
    m_leadColor = color;
    emit leadColorChanged();
    update();
}

void cwBusyRing::setLineWidthRatio(double ratio)
{
    if (m_lineWidthRatio == ratio) {
        return;
    }
    m_lineWidthRatio = ratio;
    emit lineWidthRatioChanged();
    update();
}

QCanvasPainterItemRenderer* cwBusyRing::createItemRenderer() const
{
    return new cwBusyRingRenderer();
}
