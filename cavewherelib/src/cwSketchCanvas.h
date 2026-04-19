/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHCANVAS_H
#define CWSKETCHCANVAS_H

//Qt includes
#include <QCanvasPainterItem>
#include <QPointF>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchPainterPathModel.h"

class cwSketch;
class cwInfiniteGridModel;
class QAbstractItemModel;

class CAVEWHERE_LIB_EXPORT cwSketchCanvas : public QCanvasPainterItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchCanvas)

    Q_PROPERTY(cwSketch* sketch READ sketch WRITE setSketch NOTIFY sketchChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QPointF pan READ pan WRITE setPan NOTIFY panChanged)
    Q_PROPERTY(int activeStrokeIndex READ activeStrokeIndex WRITE setActiveStrokeIndex NOTIFY activeStrokeIndexChanged)
    Q_PROPERTY(cwSketchPainterPathModel* pathModel READ pathModel CONSTANT)
    Q_PROPERTY(cwInfiniteGridModel* grid READ grid WRITE setGrid NOTIFY gridChanged)

public:
    explicit cwSketchCanvas(QQuickItem *parent = nullptr);
    ~cwSketchCanvas() override;

    cwSketch *sketch() const;
    void setSketch(cwSketch *sketch);

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

    QPointF pan() const { return m_pan; }
    void setPan(QPointF pan);

    int activeStrokeIndex() const;
    void setActiveStrokeIndex(int index);

    cwSketchPainterPathModel *pathModel() const { return m_pathModel; }

    cwInfiniteGridModel *grid() const { return m_grid; }
    void setGrid(cwInfiniteGridModel *grid);

protected:
    QCanvasPainterItemRenderer *createItemRenderer() const override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

signals:
    void sketchChanged();
    void zoomChanged();
    void panChanged();
    void activeStrokeIndexChanged();
    void gridChanged();

private:
    cwSketch *m_sketch = nullptr;
    cwSketchPainterPathModel *m_pathModel = nullptr;
    cwInfiniteGridModel *m_grid = nullptr;
    double m_zoom = 1.0;
    QPointF m_pan;

    void connectPathModelSignals();
    void connectModelForUpdate(QAbstractItemModel *model);
    void disconnectGridModels(cwInfiniteGridModel *grid);
    void connectGridModels(cwInfiniteGridModel *grid);
    void updateGridView();
};

#endif // CWSKETCHCANVAS_H
