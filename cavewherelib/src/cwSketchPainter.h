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
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGridTextModel.h"

class cwSketchDraw;
class cwAbstractSketchPainterPathModel;

class CAVEWHERE_LIB_EXPORT cwSketchPainter
{
public:
    struct GridLayer {
        const cwAbstractSketchPainterPathModel *paths = nullptr;
        const QVector<cwGridTextModel::TextRow> *text = nullptr;
    };

    struct PaintContext {
        QRectF viewport;
        double zoom = 1.0;
        const cwAbstractSketchPainterPathModel *strokes = nullptr;

        // Optional grid layer. Drawn before strokes; major on top of minor.
        GridLayer gridMinor;
        GridLayer gridMajor;
    };

    static void paint(cwSketchDraw *draw, const PaintContext &context);
};

#endif // CWSKETCHPAINTER_H
