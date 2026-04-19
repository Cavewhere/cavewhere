/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchCanvas.h"
#include "cwSketch.h"
#include "cwSketchCanvasRenderer.h"
#include "cwSketchPainterPathModel.h"
#include "cwPenStrokeModel.h"
#include "cwInfiniteGridModel.h"
#include "cwFixedGridModel.h"
#include "cwGridTextModel.h"

//Qt includes
#include <QAbstractItemModel>
#include <QTransform>

cwSketchCanvas::cwSketchCanvas(QQuickItem *parent)
    : QCanvasPainterItem(parent),
      m_pathModel(new cwSketchPainterPathModel(this))
{
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
    connectPathModelSignals();
}

cwSketchCanvas::~cwSketchCanvas() = default;

cwSketch *cwSketchCanvas::sketch() const
{
    return m_sketch;
}

void cwSketchCanvas::setSketch(cwSketch *sketch)
{
    if (m_sketch == sketch) {
        return;
    }

    if (m_sketch) {
        disconnect(m_sketch, nullptr, this, nullptr);
    }

    m_sketch = sketch;

    if (m_sketch != nullptr) {
        m_pathModel->setStrokeModel(m_sketch->strokeModel());
        connect(m_sketch, &cwSketch::strokesReset,
                this, [this]() { update(); });
        connect(m_sketch, &QObject::destroyed,
                this, [this]() { setSketch(nullptr); });
    } else {
        m_pathModel->setStrokeModel(nullptr);
    }

    emit sketchChanged();
    update();
}

void cwSketchCanvas::setZoom(double zoom)
{
    if (qFuzzyCompare(m_zoom, zoom)) {
        return;
    }
    m_zoom = zoom;
    updateGridView();
    emit zoomChanged();
    update();
}

void cwSketchCanvas::setPan(QPointF pan)
{
    if (m_pan == pan) {
        return;
    }
    m_pan = pan;
    updateGridView();
    emit panChanged();
    update();
}

int cwSketchCanvas::activeStrokeIndex() const
{
    return m_pathModel->activeStrokeIndex();
}

void cwSketchCanvas::setActiveStrokeIndex(int index)
{
    if (m_pathModel->activeStrokeIndex() == index) {
        return;
    }
    m_pathModel->setActiveStrokeIndex(index);
    emit activeStrokeIndexChanged();
    update();
}

void cwSketchCanvas::setMapMatrix(const QMatrix4x4 &matrix)
{
    if (m_mapMatrix == matrix) {
        return;
    }
    m_mapMatrix = matrix;
    updateGridView();
    emit mapMatrixChanged();
    update();
}

void cwSketchCanvas::setGrid(cwInfiniteGridModel *grid)
{
    if (m_grid == grid) {
        return;
    }

    disconnectGridModels(m_grid);
    m_grid = grid;
    connectGridModels(m_grid);
    updateGridView();

    emit gridChanged();
    update();
}

QCanvasPainterItemRenderer *cwSketchCanvas::createItemRenderer() const
{
    return new cwSketchCanvasRenderer();
}

void cwSketchCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QCanvasPainterItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        updateGridView();
    }
}

void cwSketchCanvas::connectPathModelSignals()
{
    connectModelForUpdate(m_pathModel);
}

void cwSketchCanvas::connectModelForUpdate(QAbstractItemModel *model)
{
    if (model == nullptr) {
        return;
    }
    auto requestUpdate = [this]() { update(); };
    connect(model, &QAbstractItemModel::dataChanged,   this, requestUpdate);
    connect(model, &QAbstractItemModel::rowsInserted,  this, requestUpdate);
    connect(model, &QAbstractItemModel::rowsRemoved,   this, requestUpdate);
    connect(model, &QAbstractItemModel::modelReset,    this, requestUpdate);
    connect(model, &QAbstractItemModel::layoutChanged, this, requestUpdate);
}

void cwSketchCanvas::disconnectGridModels(cwInfiniteGridModel *grid)
{
    if (grid == nullptr) {
        return;
    }
    disconnect(grid, nullptr, this, nullptr);
    if (auto *m = grid->majorGridModel()) { disconnect(m, nullptr, this, nullptr); }
    if (auto *m = grid->minorGridModel()) { disconnect(m, nullptr, this, nullptr); }
    if (auto *m = grid->majorTextModel()) { disconnect(m, nullptr, this, nullptr); }
    if (auto *m = grid->minorTextModel()) { disconnect(m, nullptr, this, nullptr); }
}

void cwSketchCanvas::connectGridModels(cwInfiniteGridModel *grid)
{
    if (grid == nullptr) {
        return;
    }
    connect(grid, &QObject::destroyed, this, [this]() { setGrid(nullptr); });
    connectModelForUpdate(grid->majorGridModel());
    connectModelForUpdate(grid->minorGridModel());
    connectModelForUpdate(grid->majorTextModel());
    connectModelForUpdate(grid->minorTextModel());
}

void cwSketchCanvas::updateGridView()
{
    if (m_grid == nullptr) {
        return;
    }

    // viewScale = 1/zoom is the pen-width / label-scale compensation factor
    // and drives the zoom-level log10 heuristic paired with mapMatrix scale.
    m_grid->setViewScale(m_zoom > 0.0 ? 1.0 / m_zoom : 1.0);

    if (m_zoom <= 0.0) {
        return;
    }

    const QTransform worldToItem =
        m_mapMatrix.toTransform()
        * QTransform().scale(m_zoom, m_zoom)
        * QTransform().translate(m_pan.x(), m_pan.y());

    bool invertible = false;
    const QTransform itemToWorld = worldToItem.inverted(&invertible);
    if (!invertible) {
        return;
    }

    const QRectF itemRect(0.0, 0.0, width(), height());
    const QRectF worldViewport = itemToWorld.mapRect(itemRect);
    const QPointF worldOrigin = itemToWorld.map(QPointF(0.0, 0.0));
    m_grid->setViewport(worldViewport);
    m_grid->setGridOrigin(worldOrigin);
}
