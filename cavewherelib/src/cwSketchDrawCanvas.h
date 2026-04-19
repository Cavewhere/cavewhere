/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHDRAWCANVAS_H
#define CWSKETCHDRAWCANVAS_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchDraw.h"

class QCanvasPainter;

class CAVEWHERE_LIB_EXPORT cwSketchDrawCanvas : public cwSketchDraw
{
public:
    explicit cwSketchDrawCanvas(QCanvasPainter *painter);

    void save() override;
    void restore() override;
    void setTransform(const QTransform &transform) override;
    void translate(double dx, double dy) override;
    void scale(double sx, double sy) override;
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
    QCanvasPainter *m_painter;
};

#endif // CWSKETCHDRAWCANVAS_H
