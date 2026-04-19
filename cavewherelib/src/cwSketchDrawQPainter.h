/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHDRAWQPAINTER_H
#define CWSKETCHDRAWQPAINTER_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchDraw.h"

class QPainter;

class CAVEWHERE_LIB_EXPORT cwSketchDrawQPainter : public cwSketchDraw
{
public:
    explicit cwSketchDrawQPainter(QPainter *painter);

    void save() override;
    void restore() override;
    void setTransform(const QTransform &transform) override;
    void setClipRect(const QRectF &rect) override;
    void setStrokePen(const QColor &color, double widthPx,
                      Qt::PenCapStyle cap,
                      Qt::PenJoinStyle join) override;
    void setFillBrush(const QColor &color) override;
    void strokePath(const QPainterPath &path) override;
    void fillPath(const QPainterPath &path) override;
    void drawText(const QPointF &pos, const QString &text,
                  const QFont &font, const QColor &color) override;

private:
    QPainter *m_painter;
};

#endif // CWSKETCHDRAWQPAINTER_H
