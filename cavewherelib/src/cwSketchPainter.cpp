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

namespace {

void drawPaths(cwSketchDraw *draw, const cwAbstractSketchPainterPathModel *model)
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
            draw->setStrokePen(color, width, Qt::RoundCap, Qt::RoundJoin);
            draw->strokePath(path);
        } else {
            draw->setFillBrush(color);
            draw->fillPath(path);
        }
    }
}

void drawText(cwSketchDraw *draw,
              const QVector<cwGridTextModel::TextRow> *rows,
              double labelScale)
{
    if (rows == nullptr || labelScale <= 0.0) {
        return;
    }
    for (const auto &row : *rows) {
        if (row.text.isEmpty()) {
            continue;
        }
        // Counter-scale around the text anchor so the font renders at its
        // intrinsic screen size regardless of the painter's zoom transform.
        // Label bounds in cwFixedGridModel are already pre-shrunk by the same
        // labelScale so positions land correctly after this.
        // row.position is the bounding rect's top-left; drawText uses the
        // baseline, so offset down by the font ascent.
        const QFontMetricsF metrics(row.font);
        draw->save();
        draw->translate(row.position.x(), row.position.y());
        draw->scale(labelScale, labelScale);
        draw->drawText(QPointF(0.0, metrics.ascent()), row.text, row.font, row.fillColor);
        draw->restore();
    }
}

} // namespace

void cwSketchPainter::paint(cwSketchDraw *draw, const PaintContext &context)
{
    Q_ASSERT(draw != nullptr);

    draw->save();

    if (!context.viewport.isNull()) {
        draw->setClipRect(context.viewport);
    }

    const double labelScale = context.zoom > 0.0 ? 1.0 / context.zoom : 1.0;

    drawPaths(draw, context.gridMinor.paths);
    drawPaths(draw, context.gridMajor.paths);
    drawText (draw, context.gridMinor.text, labelScale);
    drawText (draw, context.gridMajor.text, labelScale);
    drawPaths(draw, context.strokes);

    draw->restore();
}
