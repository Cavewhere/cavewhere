/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwProgressRing.h"
#include "cwProgressRingRenderer.h"

cwProgressRing::cwProgressRing(QQuickItem* parent) :
    QCanvasPainterItem(parent)
{
    //A ring is mostly the thing behind it. QCanvasPainterItem fills its whole
    //rect with fillColor, which defaults to opaque black, and composites the
    //result opaquely unless alphaBlending is on — so both are needed, and either
    //one alone still paints a black tile.
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
}

cwProgressRing::~cwProgressRing() = default;

void cwProgressRing::setProgress(double progress)
{
    if (m_progress == progress) {
        return;
    }
    m_progress = progress;
    emit progressChanged();
    update();
}

void cwProgressRing::setSpinAngle(double spinAngle)
{
    if (m_spinAngle == spinAngle) {
        return;
    }
    m_spinAngle = spinAngle;
    emit spinAngleChanged();
    update();
}

void cwProgressRing::setTrackColor(QColor color)
{
    if (m_trackColor == color) {
        return;
    }
    m_trackColor = color;
    emit trackColorChanged();
    update();
}

void cwProgressRing::setArcColor(QColor color)
{
    if (m_arcColor == color) {
        return;
    }
    m_arcColor = color;
    emit arcColorChanged();
    update();
}

QCanvasPainterItemRenderer* cwProgressRing::createItemRenderer() const
{
    return new cwProgressRingRenderer();
}
