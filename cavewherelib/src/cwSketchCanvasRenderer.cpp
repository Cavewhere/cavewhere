/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchCanvasRenderer.h"
#include "cwSketchCanvas.h"
#include "cwSketchDrawCanvas.h"
#include "cwSketchPainter.h"
#include "cwAbstractSketchPainterPathModel.h"
#include "cwCenterlineSketchPainterModel.h"
#include "cwInfiniteGridModel.h"
#include "cwFixedGridModel.h"
#include "cwGridTextModel.h"

//Qt includes
#include <QCanvasPainter>
#include <QCanvasPainterItem>

// Render-thread-owned model backing cwSketchPainter::paint. Populated in
// synchronize() while the GUI thread is blocked; read in paint() without
// touching the live cwSketchPainterPathModel.
class cwSketchCanvasRendererSnapshotModel final : public cwAbstractSketchPainterPathModel
{
public:
    using cwAbstractSketchPainterPathModel::Path;

    QList<Path> entries;

    int rowCount(const QModelIndex & = QModelIndex()) const override {
        return static_cast<int>(entries.size());
    }

protected:
    Path path(const QModelIndex &index) const override {
        return entries.at(index.row());
    }
};

namespace {

void snapshotPaths(const cwAbstractSketchPainterPathModel *source,
                   cwSketchCanvasRendererSnapshotModel *target)
{
    target->entries.clear();
    if (source == nullptr) {
        return;
    }
    const int rows = source->rowCount();
    target->entries.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx = source->index(row, 0);
        const QPainterPath painterPath =
            idx.data(cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
        if (painterPath.isEmpty()) {
            continue;
        }
        target->entries.append({
            painterPath,
            idx.data(cwAbstractSketchPainterPathModel::StrokeColorRole).value<QColor>(),
            idx.data(cwAbstractSketchPainterPathModel::StrokeWidthRole).toDouble(),
            0.0
        });
    }
}

} // namespace

cwSketchCanvasRenderer::cwSketchCanvasRenderer()
    : m_snapshot(new cwSketchCanvasRendererSnapshotModel()),
      m_minorGridSnapshot(new cwSketchCanvasRendererSnapshotModel()),
      m_majorGridSnapshot(new cwSketchCanvasRendererSnapshotModel()),
      m_linePlotSnapshot(new cwSketchCanvasRendererSnapshotModel())
{
}

cwSketchCanvasRenderer::~cwSketchCanvasRenderer()
{
    delete m_snapshot;
    delete m_minorGridSnapshot;
    delete m_majorGridSnapshot;
    delete m_linePlotSnapshot;
}

void cwSketchCanvasRenderer::synchronize(QCanvasPainterItem *item)
{
    auto *canvas = qobject_cast<cwSketchCanvas *>(item);
    if (canvas == nullptr) {
        m_snapshot->entries.clear();
        m_minorGridSnapshot->entries.clear();
        m_majorGridSnapshot->entries.clear();
        m_linePlotSnapshot->entries.clear();
        m_minorTextSnapshot.clear();
        m_majorTextSnapshot.clear();
        return;
    }

    const QPointF pan = canvas->pan();
    const QMatrix4x4 mapMatrix = canvas->mapMatrix();
    const double userZoom = canvas->zoom();
    m_mapScale = mapMatrix(0, 0);
    m_worldToItem = mapMatrix.toTransform()
                    * QTransform().scale(userZoom, userZoom)
                    * QTransform().translate(pan.x(), pan.y());

    if (userZoom > 0.0) {
        const QRectF itemRect(0.0, 0.0, canvas->width(), canvas->height());
        m_worldViewport = m_worldToItem.inverted().mapRect(itemRect);
    } else {
        m_worldViewport = QRectF();
    }

    snapshotPaths(canvas->pathModel(), m_snapshot);
    snapshotPaths(canvas->linePlotModel(), m_linePlotSnapshot);

    auto *grid = canvas->grid();
    snapshotPaths(grid ? grid->minorGridModel() : nullptr, m_minorGridSnapshot);
    snapshotPaths(grid ? grid->majorGridModel() : nullptr, m_majorGridSnapshot);
    m_minorTextSnapshot = (grid && grid->minorTextModel())
                              ? grid->minorTextModel()->rows()
                              : QVector<cwGridTextModel::TextRow>();
    m_majorTextSnapshot = (grid && grid->majorTextModel())
                              ? grid->majorTextModel()->rows()
                              : QVector<cwGridTextModel::TextRow>();
}

void cwSketchCanvasRenderer::paint(QCanvasPainter *painter)
{
    const float w = width();
    const float h = height();
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }

    painter->clearRect(0.0f, 0.0f, w, h);
    painter->setRenderHint(QCanvasPainter::RenderHint::Antialiasing);

    const bool hasStrokes  = !m_snapshot->entries.isEmpty();
    const bool hasLinePlot = !m_linePlotSnapshot->entries.isEmpty();
    const bool hasGrid     = !m_minorGridSnapshot->entries.isEmpty()
                          || !m_majorGridSnapshot->entries.isEmpty()
                          || !m_minorTextSnapshot.isEmpty()
                          || !m_majorTextSnapshot.isEmpty();

    if (!hasStrokes && !hasGrid && !hasLinePlot) {
        return;
    }

    painter->save();
    cwSketchDrawCanvas draw(painter);

    cwSketchPainter::PaintContext ctx;
    ctx.viewport       = m_worldViewport;
    ctx.worldToItem    = m_worldToItem;
    ctx.mapScale       = m_mapScale;
    ctx.strokes        = m_snapshot;
    ctx.gridMinor.paths = m_minorGridSnapshot;
    ctx.gridMinor.text  = &m_minorTextSnapshot;
    ctx.gridMajor.paths = m_majorGridSnapshot;
    ctx.gridMajor.text  = &m_majorTextSnapshot;
    ctx.linePlot        = m_linePlotSnapshot;
    cwSketchPainter::paint(&draw, ctx);

    painter->restore();
}
