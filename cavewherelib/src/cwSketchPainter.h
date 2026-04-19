/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHPAINTER_H
#define CWSKETCHPAINTER_H

//Qt includes
#include <QRectF>

//Our includes
#include "CaveWhereLibExport.h"

class cwSketchDraw;
class cwAbstractSketchPainterPathModel;

class CAVEWHERE_LIB_EXPORT cwSketchPainter
{
public:
    struct PaintContext {
        QRectF viewport;
        double zoom = 1.0;
        const cwAbstractSketchPainterPathModel *strokes = nullptr;
    };

    static void paint(cwSketchDraw *draw, const PaintContext &context);
};

#endif // CWSKETCHPAINTER_H
