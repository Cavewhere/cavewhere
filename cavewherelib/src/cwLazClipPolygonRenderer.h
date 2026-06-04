/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZCLIPPOLYGONRENDERER_H
#define CWLAZCLIPPOLYGONRENDERER_H

//Qt includes
#include <QCanvasPainterItemRenderer>
#include <QColor>
#include <QPointF>
#include <QPolygonF>

//Our includes
#include "cwLazClipInteraction.h"

class cwLazClipPolygonRenderer : public QCanvasPainterItemRenderer
{
public:
    cwLazClipPolygonRenderer();
    ~cwLazClipPolygonRenderer() override;

    void synchronize(QCanvasPainterItem* item) override;
    void paint(QCanvasPainter* painter) override;

private:
    QPolygonF m_polygonScreen;
    QPointF m_hoverScreenPos;
    cwLazClipInteraction::State m_state = cwLazClipInteraction::State::Idle;
    bool m_snapActive = false;

    QColor m_outlineColor;
    QColor m_fillColor;
    QColor m_vertexBorderColor;
};

#endif // CWLAZCLIPPOLYGONRENDERER_H
