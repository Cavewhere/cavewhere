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
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSketchPainterPathModel.h"

class cwSketch;

class CAVEWHERE_LIB_EXPORT cwSketchCanvas : public QCanvasPainterItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchCanvas)

    Q_PROPERTY(cwSketch* sketch READ sketch WRITE setSketch NOTIFY sketchChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(double panX READ panX WRITE setPanX NOTIFY panXChanged)
    Q_PROPERTY(double panY READ panY WRITE setPanY NOTIFY panYChanged)
    Q_PROPERTY(int activeStrokeIndex READ activeStrokeIndex WRITE setActiveStrokeIndex NOTIFY activeStrokeIndexChanged)
    Q_PROPERTY(cwSketchPainterPathModel* pathModel READ pathModel CONSTANT)

public:
    explicit cwSketchCanvas(QQuickItem *parent = nullptr);
    ~cwSketchCanvas() override;

    cwSketch *sketch() const;
    void setSketch(cwSketch *sketch);

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

    double panX() const { return m_panX; }
    void setPanX(double x);

    double panY() const { return m_panY; }
    void setPanY(double y);

    int activeStrokeIndex() const;
    void setActiveStrokeIndex(int index);

    cwSketchPainterPathModel *pathModel() const { return m_pathModel; }

protected:
    QCanvasPainterItemRenderer *createItemRenderer() const override;

signals:
    void sketchChanged();
    void zoomChanged();
    void panXChanged();
    void panYChanged();
    void activeStrokeIndexChanged();

private:
    cwSketch *m_sketch = nullptr;
    cwSketchPainterPathModel *m_pathModel = nullptr;
    double m_zoom = 1.0;
    double m_panX = 0.0;
    double m_panY = 0.0;

    void connectPathModelSignals();
};

#endif // CWSKETCHCANVAS_H
