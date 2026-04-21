/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHCANVASRENDERER_H
#define CWSKETCHCANVASRENDERER_H

//Qt includes
#include <QCanvasPainterItemRenderer>
#include <QRectF>
#include <QTransform>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGridTextModel.h"
#include "cwSketchPainter.h"
#include "cwScrapManager.h"

class cwSketchCanvasRendererSnapshotModel;

class CAVEWHERE_LIB_EXPORT cwSketchCanvasRenderer : public QCanvasPainterItemRenderer
{
public:
    cwSketchCanvasRenderer();
    ~cwSketchCanvasRenderer() override;

    void synchronize(QCanvasPainterItem *item) override;
    void paint(QCanvasPainter *painter) override;

private:
    cwSketchCanvasRendererSnapshotModel *m_snapshot;
    cwSketchCanvasRendererSnapshotModel *m_minorGridSnapshot;
    cwSketchCanvasRendererSnapshotModel *m_majorGridSnapshot;
    cwSketchCanvasRendererSnapshotModel *m_linePlotSnapshot;
    QVector<cwGridTextModel::TextRow> m_minorTextSnapshot;
    QVector<cwGridTextModel::TextRow> m_majorTextSnapshot;
    QVector<cwGridTextModel::TextRow> m_linePlotTextSnapshot;
    QTransform m_worldToItem;
    QRectF m_worldViewport;
    double m_mapScale = 1.0;
    double m_userZoom = 1.0;
    double m_mapScaleRatio = cwSketchPainter::LinePlotReferenceMapScaleRatio;

    QVector<cwScrapManager::SketchScrapDebugEntry> m_debugSnapshot;
};

#endif // CWSKETCHCANVASRENDERER_H
