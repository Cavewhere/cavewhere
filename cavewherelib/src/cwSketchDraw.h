/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHDRAW_H
#define CWSKETCHDRAW_H

//Qt includes
#include <QColor>
#include <QFont>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <Qt>

//Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwSketchDraw
{
public:
    virtual ~cwSketchDraw() = default;

    virtual void save()    = 0;
    virtual void restore() = 0;
    virtual void setTransform(const QTransform &transform)    = 0;
    virtual void translate(double dx, double dy)              = 0;
    virtual void scale(double sx, double sy)                  = 0;
    virtual void setClipRect(const QRectF &rect)              = 0;
    virtual void setStrokePen(const QColor &color, double widthPx,
                              Qt::PenCapStyle cap,
                              Qt::PenJoinStyle join)          = 0;
    virtual void setFillBrush(const QColor &color)            = 0;
    virtual void strokePath(const QPainterPath &path)         = 0;
    virtual void fillPath(const QPainterPath &path)           = 0;
    virtual void drawText(const QPointF &pos, const QString &text,
                          const QFont &font, const QColor &color) = 0;
};

#endif // CWSKETCHDRAW_H
