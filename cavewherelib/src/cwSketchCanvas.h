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
#include <QMatrix4x4>
#include <QPointer>
#include <QPointF>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchManager.h"
#include "cwSketchPainterPathModel.h"
#include "cwTripLinePlotTask.h"

class cwSketch;
class cwInfiniteGridModel;
class cwCenterlineSketchPainterModel;
class cwSurvey2DGeometryArtifact;
class cwTrip;
class cwScrapManager;
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
    Q_PROPERTY(QMatrix4x4 mapMatrix READ mapMatrix WRITE setMapMatrix NOTIFY mapMatrixChanged)
    Q_PROPERTY(cwSketchManager* sketchManager READ sketchManager WRITE setSketchManager NOTIFY sketchManagerChanged)
    Q_PROPERTY(cwCenterlineSketchPainterModel* linePlotModel READ linePlotModel CONSTANT)
    Q_PROPERTY(cwScrapManager* scrapManager READ scrapManager WRITE setScrapManager NOTIFY scrapManagerChanged)
    Q_PROPERTY(bool debugOverlayVisible READ debugOverlayVisible WRITE setDebugOverlayVisible NOTIFY debugOverlayVisibleChanged)

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

    QMatrix4x4 mapMatrix() const { return m_mapMatrix; }
    void setMapMatrix(const QMatrix4x4 &matrix);

    cwSketchManager *sketchManager() const { return m_sketchManager; }
    void setSketchManager(cwSketchManager *manager);

    cwCenterlineSketchPainterModel *linePlotModel() const { return m_linePlotModel; }

    cwScrapManager *scrapManager() const { return m_scrapManager; }
    void setScrapManager(cwScrapManager *manager);

    bool debugOverlayVisible() const;
    void setDebugOverlayVisible(bool visible);

protected:
    QCanvasPainterItemRenderer *createItemRenderer() const override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

signals:
    void sketchChanged();
    void zoomChanged();
    void panChanged();
    void activeStrokeIndexChanged();
    void gridChanged();
    void mapMatrixChanged();
    void sketchManagerChanged();
    void scrapManagerChanged();
    void debugOverlayVisibleChanged();

    // Emitted when the sketch's current trip resolves to >1 connected
    // component AND the sketch has no valid anchor. The canvas does NOT
    // render the line plot until the caller sets sketch->anchorStation()
    // in response.
    void requestAnchorSelection(QList<cwTripComponentSummary> components);

private:
    cwSketch *m_sketch = nullptr;
    cwSketchPainterPathModel *m_pathModel = nullptr;
    cwInfiniteGridModel *m_grid = nullptr;
    double m_zoom = 1.0;
    QPointF m_pan;
    QMatrix4x4 m_mapMatrix;

    QPointer<cwSketchManager> m_sketchManager;
    QPointer<cwScrapManager> m_scrapManager;
    QMetaObject::Connection m_scrapDebugChangedConnection;
    QMetaObject::Connection m_viewStateDebugChangedConnection;
    cwCenterlineSketchPainterModel *m_linePlotModel = nullptr;
    cwSurvey2DGeometryArtifact *m_linePlotGeometry = nullptr;
    QPointer<cwTrip> m_acquiredTrip;
    QMetaObject::Connection m_linePlotUpdatedConnection;
    QMetaObject::Connection m_anchorStationChangedConnection;

    void connectPathModelSignals();
    void connectModelForUpdate(QAbstractItemModel *model);
    void disconnectGridModels(cwInfiniteGridModel *grid);
    void connectGridModels(cwInfiniteGridModel *grid);
    void updateGridView();

    void acquireLinePlotForSketch(cwSketch *sketch);
    void releaseLinePlotAcquisition();
    void refreshLinePlotFromManager();
    void clearLinePlotGeometry();
};

#endif // CWSKETCHCANVAS_H
