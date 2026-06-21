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
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchManager.h"
#include "cwDecoratedStrokePathSource.h"
#include "cwTripLinePlotTask.h"

class cwSketch;
class cwScale;
class cwInfiniteGridModel;
class cwFixedGridModel;
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
    Q_PROPERTY(QString currentBrushName READ currentBrushName WRITE setCurrentBrushName NOTIFY currentBrushNameChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QPointF pan READ pan WRITE setPan NOTIFY panChanged)
    Q_PROPERTY(int activeStrokeIndex READ activeStrokeIndex WRITE setActiveStrokeIndex NOTIFY activeStrokeIndexChanged)
    Q_PROPERTY(cwDecoratedStrokePathSource* pathModel READ pathModel CONSTANT)
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

    // The brush every new stroke is drawn with: the single source of truth
    // shared by the QML brush picker, the keyboard shortcuts, and the
    // beginStroke() call site. Defaults to the wall brush.
    QString currentBrushName() const { return m_currentBrushName; }
    void setCurrentBrushName(const QString &name);

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

    QPointF pan() const { return m_pan; }
    void setPan(QPointF pan);

    int activeStrokeIndex() const;
    void setActiveStrokeIndex(int index);

    cwDecoratedStrokePathSource *pathModel() const { return m_pathModel; }

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

    // Authoring preview override (commit 9 brush editor). While a brush is being
    // edited, the editor pushes a preview snapshot — the resolved palette with the
    // working brush swapped in — so the live sketch re-skins on every edit without
    // mutating the real palette. clearPreviewSnapshot() drops the override and
    // re-resolves from the sketch (e.g. after Apply persisted the real brush, or
    // on Discard). Not persisted; ignored by cwSketchGlyphCanvas, which fully
    // overrides snapshotForPathModel().
    void setPreviewSnapshot(const cwPaletteSnapshot &snapshot);
    void clearPreviewSnapshot();

protected:
    QCanvasPainterItemRenderer *createItemRenderer() const override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

    // The palette snapshot pushed into the path model. The base resolves it from
    // the sketch's region-resolved palette; cwSketchGlyphCanvas overrides it to
    // inject an authoring palette directly, bypassing the resolver. Call
    // refreshPathSnapshot() to re-push after the source changes.
    virtual cwPaletteSnapshot snapshotForPathModel() const;
    void refreshPathSnapshot();

signals:
    void sketchChanged();
    void currentBrushNameChanged();
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
    QString m_currentBrushName;
    cwDecoratedStrokePathSource *m_pathModel = nullptr;
    cwPaletteSnapshot m_previewSnapshot;
    bool m_hasPreviewSnapshot = false;
    cwInfiniteGridModel *m_grid = nullptr;
    double m_zoom = 1.0;
    QPointF m_pan;
    QMatrix4x4 m_mapMatrix;

    QMetaObject::Connection m_mapScaleConnection;
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
    void connectModelForUpdate(cwFixedGridModel *model);
    void connectModelForUpdate(cwCenterlineSketchPainterModel *model);
    void disconnectGridModels(cwInfiniteGridModel *grid);
    void connectGridModels(cwInfiniteGridModel *grid);
    void updateGridView();

    void acquireLinePlotForSketch(cwSketch *sketch);
    void releaseLinePlotAcquisition();
    void refreshLinePlotFromManager();
    void clearLinePlotGeometry();
};

#endif // CWSKETCHCANVAS_H
