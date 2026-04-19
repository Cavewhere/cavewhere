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

//Our includes
#include "CaveWhereLibExport.h"

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
    QTransform m_worldToItem;
    QRectF m_worldViewport;
};

#endif // CWSKETCHCANVASRENDERER_H
