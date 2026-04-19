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

//Qt includes
#include <QAbstractItemModel>

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
    emit zoomChanged();
    update();
}

void cwSketchCanvas::setPan(QPointF pan)
{
    if (m_pan == pan) {
        return;
    }
    m_pan = pan;
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

QCanvasPainterItemRenderer *cwSketchCanvas::createItemRenderer() const
{
    return new cwSketchCanvasRenderer();
}

void cwSketchCanvas::connectPathModelSignals()
{
    auto requestUpdate = [this]() { update(); };
    connect(m_pathModel, &QAbstractItemModel::dataChanged,   this, requestUpdate);
    connect(m_pathModel, &QAbstractItemModel::rowsInserted,  this, requestUpdate);
    connect(m_pathModel, &QAbstractItemModel::rowsRemoved,   this, requestUpdate);
    connect(m_pathModel, &QAbstractItemModel::modelReset,    this, requestUpdate);
    connect(m_pathModel, &QAbstractItemModel::layoutChanged, this, requestUpdate);
}
