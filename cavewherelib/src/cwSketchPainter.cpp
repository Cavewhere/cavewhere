/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchPainter.h"
#include "cwSketchDraw.h"
#include "cwAbstractSketchPainterPathModel.h"

//Qt includes
#include <QAbstractItemModel>
#include <QFontMetricsF>
#include <QPainterPath>

#include <cmath>

namespace {

void drawPaths(cwSketchDraw *draw,
               const cwAbstractSketchPainterPathModel *model,
               double penScale)
{
    if (model == nullptr) {
        return;
    }
    const int rows = model->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx = model->index(row);
        const QPainterPath path =
            idx.data(cwAbstractSketchPainterPathModel::PainterPathRole)
                .value<QPainterPath>();
        if (path.isEmpty()) {
            continue;
        }
        const QColor color =
            idx.data(cwAbstractSketchPainterPathModel::StrokeColorRole)
                .value<QColor>();
        const double width =
            idx.data(cwAbstractSketchPainterPathModel::StrokeWidthRole)
                .toDouble();

        if (width > 0.0) {
            draw->setStrokePen(color, width * penScale, Qt::RoundCap, Qt::RoundJoin);
            draw->strokePath(path);
        } else {
            draw->setFillBrush(color);
            draw->fillPath(path);
        }
    }
}

// Labels live at world-meter positions but must render at the font's native
// screen-pixel size regardless of mapScale or user zoom. Map each label's
// world bounds rect to screen pixels, derive the baseline from the screen
// rect's top + ascent (independent of the world's Y direction), reset the
// painter transform to identity, and draw at native font size.
void drawText(cwSketchDraw *draw,
              const QVector<cwGridTextModel::TextRow> *rows,
              const QTransform &worldToItem,
              double fontScale = 1.0)
{
    if (rows == nullptr || rows->isEmpty()) {
        return;
    }
    // Guard against mis-wired callers: non-finite / non-positive scale
    // would produce zero-size glyphs or NaN metrics.
    if (!std::isfinite(fontScale) || fontScale <= 0.0) {
        fontScale = 1.0;
    }
    QFont cachedRowFont;
    QFont cachedDrawFont;
    QFontMetricsF metrics(cachedDrawFont);
    bool metricsValid = false;
    for (const auto &row : *rows) {
        if (row.text.isEmpty()) {
            continue;
        }
        if (!metricsValid || row.font != cachedRowFont) {
            cachedRowFont = row.font;
            cachedDrawFont = row.font;
            if (fontScale != 1.0) {
                cachedDrawFont.setPointSizeF(row.font.pointSizeF() * fontScale);
            }
            metrics = QFontMetricsF(cachedDrawFont);
            metricsValid = true;
        }
        const QRectF screenBounds = worldToItem.mapRect(row.bounds);
        const QRectF glyphBR = metrics.boundingRect(row.text);
        // Qt drawText anchors at the baseline: glyphBR.top() is negative
        // (≈ -ascent), so subtracting shifts the baseline down from the
        // rect's screen-top.
        const QPointF screenBaseline(screenBounds.left() - glyphBR.left(),
                                     screenBounds.top()  - glyphBR.top());
        draw->save();
        draw->setTransform(QTransform());
        draw->drawText(screenBaseline, row.text, cachedDrawFont, row.fillColor);
        draw->restore();
    }
}

} // namespace

double cwSketchPainter::pixelsPerMeterFromPaperScale(double paperScale, double dpi)
{
    constexpr double kInchesPerMeter = 1000.0 / 25.4;
    return paperScale * dpi * kInchesPerMeter;
}

QTransform cwSketchPainter::paperTransform(double mapScale, double dpi)
{
    const double k = pixelsPerMeterFromPaperScale(mapScale, dpi);
    return QTransform::fromScale(k, -k);
}

double cwSketchPainter::paperStrokePenScale(double paperScale, double dpi)
{
    const double ppm = pixelsPerMeterFromPaperScale(paperScale, dpi);
    return ppm > 0.0 ? 1.0 / ppm : 0.0;
}

void cwSketchPainter::paint(cwSketchDraw *draw, const PaintContext &context)
{
    Q_ASSERT(draw != nullptr);
    // Paths are stored world-Y-up; worldToItem must include the Y-flip.
    Q_ASSERT(context.worldToItem.determinant() < 0.0);

    draw->save();
    draw->setTransform(context.worldToItem);

    if (!context.viewport.isNull()) {
        draw->setClipRect(context.viewport);
    }

    // Pen widths are in screen pixels. The render transform scales them by
    // mapScale · userZoom; cancel the mapScale component so drawings come out
    // at their intended screen thickness, while still letting user zoom
    // thicken strokes when zoomed in.
    const double penScale = context.mapScale > 0.0 ? 1.0 / context.mapScale : 1.0;

    drawPaths(draw, context.gridMinor.paths, penScale);
    drawPaths(draw, context.gridMajor.paths, penScale);
    drawText (draw, context.gridMinor.text, context.worldToItem);
    drawText (draw, context.gridMajor.text, context.worldToItem);
    drawPaths(draw, context.linePlot.paths, penScale);
    drawText (draw, context.linePlot.text, context.worldToItem, context.linePlotTextScale);
    const double strokePenScale = context.strokePenScale > 0.0
                                      ? context.strokePenScale
                                      : penScale;
    drawPaths(draw, context.strokes, strokePenScale);

    draw->restore();
}
