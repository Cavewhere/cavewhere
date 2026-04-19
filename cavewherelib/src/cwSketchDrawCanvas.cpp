/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchDrawCanvas.h"

//Qt includes
#include <QCanvasPainter>

namespace {

QCanvasPainter::LineCap toCanvasLineCap(Qt::PenCapStyle cap)
{
    switch (cap) {
    case Qt::FlatCap:   return QCanvasPainter::LineCap::Butt;
    case Qt::SquareCap: return QCanvasPainter::LineCap::Square;
    case Qt::RoundCap:
    default:            return QCanvasPainter::LineCap::Round;
    }
}

QCanvasPainter::LineJoin toCanvasLineJoin(Qt::PenJoinStyle join)
{
    switch (join) {
    case Qt::BevelJoin: return QCanvasPainter::LineJoin::Bevel;
    case Qt::MiterJoin:
    case Qt::SvgMiterJoin: return QCanvasPainter::LineJoin::Miter;
    case Qt::RoundJoin:
    default:            return QCanvasPainter::LineJoin::Round;
    }
}

} // namespace

cwSketchDrawCanvas::cwSketchDrawCanvas(QCanvasPainter *painter)
    : m_painter(painter)
{
    Q_ASSERT(m_painter != nullptr);
}

void cwSketchDrawCanvas::save()
{
    m_painter->save();
}

void cwSketchDrawCanvas::restore()
{
    m_painter->restore();
}

void cwSketchDrawCanvas::setTransform(const QTransform &transform)
{
    m_painter->setTransform(transform);
}

void cwSketchDrawCanvas::translate(double dx, double dy)
{
    m_painter->translate(static_cast<float>(dx), static_cast<float>(dy));
}

void cwSketchDrawCanvas::scale(double sx, double sy)
{
    m_painter->scale(static_cast<float>(sx), static_cast<float>(sy));
}

void cwSketchDrawCanvas::setClipRect(const QRectF &rect)
{
    m_painter->setClipRect(rect);
}

void cwSketchDrawCanvas::setStrokePen(const QColor &color, double widthPx,
                                      Qt::PenCapStyle cap,
                                      Qt::PenJoinStyle join)
{
    m_painter->setStrokeStyle(color);
    m_painter->setLineWidth(static_cast<float>(widthPx));
    m_painter->setLineCap(toCanvasLineCap(cap));
    m_painter->setLineJoin(toCanvasLineJoin(join));
}

void cwSketchDrawCanvas::setFillBrush(const QColor &color)
{
    m_painter->setFillStyle(color);
}

void cwSketchDrawCanvas::strokePath(const QPainterPath &path)
{
    m_painter->beginPath();
    m_painter->addPath(path);
    m_painter->stroke();
}

void cwSketchDrawCanvas::fillPath(const QPainterPath &path)
{
    m_painter->beginPath();
    m_painter->addPath(path);
    m_painter->fill();
}

void cwSketchDrawCanvas::drawText(const QPointF &pos, const QString &text,
                                  const QFont &font, const QColor &color)
{
    m_painter->save();
    m_painter->setFont(font);
    m_painter->setFillStyle(color);
    m_painter->fillText(text, static_cast<float>(pos.x()),
                        static_cast<float>(pos.y()));
    m_painter->restore();
}
