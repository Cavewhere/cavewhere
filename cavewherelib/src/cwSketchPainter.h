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
#include <QTransform>
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
        // Strokes and line-plot geometry are world-Y-up; Qt paint devices are
        // Y-down. Must include the Y-flip (determinant() < 0) or paint asserts.
        QTransform worldToItem;
        // mapMatrix(0,0). Kept separate from worldToItem because strokes are
        // specified in screen pixels and want compensation for mapScale but
        // not for userZoom (so zooming still thickens strokes).
        double mapScale = 1.0;
        const cwAbstractSketchPainterPathModel *strokes = nullptr;

        // Optional grid layer. Drawn before strokes; major on top of minor.
        GridLayer gridMinor;
        GridLayer gridMajor;

        // Optional centerline / survey line plot. Drawn between grid text
        // and user strokes so pen input overlays the reference plot.
        const cwAbstractSketchPainterPathModel *linePlot = nullptr;
    };

    // World-metre → destination-pixel transform for paper-bound render
    // targets (PDF/SVG, rasterised thumbnail). Mirrors cwWorldToScreenMatrix
    // as a pure function for callers that don't own a cwWorldToScreenMatrix.
    static QTransform paperTransform(double mapScale, double pixelDensity);

    static void paint(cwSketchDraw *draw, const PaintContext &context);
};

#endif // CWSKETCHPAINTER_H
