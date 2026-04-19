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

cwSketchCanvasRenderer::cwSketchCanvasRenderer()
    : m_snapshot(new cwSketchCanvasRendererSnapshotModel())
{
}

cwSketchCanvasRenderer::~cwSketchCanvasRenderer()
{
    delete m_snapshot;
}

void cwSketchCanvasRenderer::synchronize(QCanvasPainterItem *item)
{
    auto *canvas = qobject_cast<cwSketchCanvas *>(item);
    if (canvas == nullptr) {
        m_snapshot->entries.clear();
        return;
    }

    const QPointF pan = canvas->pan();
    m_worldToItem = QTransform()
                        .translate(pan.x(), pan.y())
                        .scale(canvas->zoom(), canvas->zoom());

    if (canvas->zoom() > 0.0) {
        const QRectF itemRect(0.0, 0.0, canvas->width(), canvas->height());
        m_worldViewport = m_worldToItem.inverted().mapRect(itemRect);
    } else {
        m_worldViewport = QRectF();
    }

    m_snapshot->entries.clear();
    const auto *pathModel = canvas->pathModel();
    if (pathModel == nullptr) {
        return;
    }

    const int rows = pathModel->rowCount();
    m_snapshot->entries.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex idx = pathModel->index(row, 0);
        const QPainterPath painterPath =
            idx.data(cwAbstractSketchPainterPathModel::PainterPathRole).value<QPainterPath>();
        if (painterPath.isEmpty()) {
            continue;
        }
        m_snapshot->entries.append({
            painterPath,
            idx.data(cwAbstractSketchPainterPathModel::StrokeColorRole).value<QColor>(),
            idx.data(cwAbstractSketchPainterPathModel::StrokeWidthRole).toDouble(),
            0.0
        });
    }
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

    if (m_snapshot->entries.isEmpty()) {
        return;
    }

    painter->save();
    cwSketchDrawCanvas draw(painter);
    draw.setTransform(m_worldToItem);

    cwSketchPainter::PaintContext ctx;
    ctx.viewport = m_worldViewport;
    ctx.zoom     = m_worldToItem.m11();
    ctx.strokes  = m_snapshot;
    cwSketchPainter::paint(&draw, ctx);

    painter->restore();
}
