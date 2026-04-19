/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchDrawQPainter.h"

//Qt includes
#include <QPainter>

cwSketchDrawQPainter::cwSketchDrawQPainter(QPainter *painter)
    : m_painter(painter)
{
    Q_ASSERT(m_painter != nullptr);
}

void cwSketchDrawQPainter::save()
{
    m_painter->save();
}

void cwSketchDrawQPainter::restore()
{
    m_painter->restore();
}

void cwSketchDrawQPainter::setTransform(const QTransform &transform)
{
    m_painter->setTransform(transform);
}

void cwSketchDrawQPainter::setClipRect(const QRectF &rect)
{
    m_painter->setClipRect(rect);
}

void cwSketchDrawQPainter::setStrokePen(const QColor &color, double widthPx,
                                        Qt::PenCapStyle cap,
                                        Qt::PenJoinStyle join)
{
    QPen pen(color);
    pen.setWidthF(widthPx);
    pen.setCapStyle(cap);
    pen.setJoinStyle(join);
    m_painter->setPen(pen);
}

void cwSketchDrawQPainter::setFillBrush(const QColor &color)
{
    m_painter->setBrush(color);
}

void cwSketchDrawQPainter::strokePath(const QPainterPath &path)
{
    // Caller already set the pen via setStrokePen; clear the brush so the
    // path is outlined only, not filled. Next setFillBrush/setStrokePen call
    // overrides this.
    m_painter->setBrush(Qt::NoBrush);
    m_painter->drawPath(path);
}

void cwSketchDrawQPainter::fillPath(const QPainterPath &path)
{
    m_painter->setPen(Qt::NoPen);
    m_painter->drawPath(path);
}

void cwSketchDrawQPainter::drawText(const QPointF &pos, const QString &text,
                                    const QFont &font, const QColor &color)
{
    m_painter->save();
    m_painter->setFont(font);
    m_painter->setPen(color);
    m_painter->drawText(pos, text);
    m_painter->restore();
}
