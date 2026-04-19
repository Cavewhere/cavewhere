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
#include <QPainterPath>

void cwSketchPainter::paint(cwSketchDraw *draw, const PaintContext &context)
{
    Q_ASSERT(draw != nullptr);

    draw->save();

    if (!context.viewport.isNull()) {
        draw->setClipRect(context.viewport);
    }

    const auto *strokes = context.strokes;
    if (strokes) {
        const int rows = strokes->rowCount();
        for (int row = 0; row < rows; ++row) {
            const QModelIndex idx = strokes->index(row);
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

    draw->restore();
}
