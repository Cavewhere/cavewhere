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
    // Map-scale ratio (paper : world) at which linePlot labels render at
    // their authored point size. Callers scale font size linearly around
    // this reference so labels at the default 1:250 look paper-sized.
    static constexpr double LinePlotReferenceMapScaleRatio = 1.0 / 250.0;

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
        // and user strokes so pen input overlays the reference plot. The
        // `text` layer (station labels) renders via the backend's native
        // text API — required so Canvas-backed renders keep character
        // counters hollow and SVG/PDF export produces selectable text.
        GridLayer linePlot;

        // Multiplier applied to linePlot text font pointSizeF at paint
        // time. 1.0 = render at the row's declared font size (same as
        // grid text). Callers compute this so centerline labels at the
        // 1:250 default render at the authored point size and scale
        // linearly with total view scale (mapScale × userZoom).
        double linePlotTextScale = 1.0;

        // Optional override for user strokes only (grid / line-plot keep
        // the default penScale derived from mapScale). <= 0 means "unset".
        // See cwSketchPainter::paperStrokePenScale to compute it.
        double strokePenScale = 0.0;
    };

    // Cave-metre → destination-pixel scalar at a given paper scale and DPI.
    //   pixels-per-cave-metre = paperScale × dpi × (1000 / 25.4)
    // paperTransform() wraps this into a Y-flipped QTransform for paint
    // devices; rasterisers that want the bare scalar call this directly.
    static double pixelsPerMeterFromPaperScale(double paperScale, double dpi);

    // PaintContext::strokePenScale value that makes pen widths map to a
    // fixed world/paper thickness (independent of screen DPI) by treating
    // cwPenStroke::width as pixels at `dpi`. Returns 0 on bad input so the
    // painter falls back to the default penScale.
    static double paperStrokePenScale(double paperScale, double dpi);

    // World-metre → destination-pixel transform for paper-bound render
    // targets (PDF/SVG, rasterised thumbnail). Mirrors cwWorldToScreenMatrix
    // as a pure function for callers that don't own a cwWorldToScreenMatrix.
    static QTransform paperTransform(double mapScale, double dpi);

    static void paint(cwSketchDraw *draw, const PaintContext &context);
};

#endif // CWSKETCHPAINTER_H
