/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMEASUREMENTLINECANVAS_H
#define CWMEASUREMENTLINECANVAS_H

//Qt includes
#include <QCanvasPainterItem>
#include <QColor>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

class cwCamera;
class cwMeasurementInteraction;

/**
 * Screen-space overlay that draws the straight line between the two measurement
 * points (and the rubber-band from A to the hover point while awaiting B). The
 * line is always drawn on top — never a 3D line, which would be occluded by
 * geometry and read wrong for an always-on annotation.
 *
 * Projection happens in the renderer's synchronize() where the GUI thread is
 * blocked, so reading the camera matrices is safe there. The marker dots
 * (A/B/hover) are separate QML items driven by a TransformUpdater; this item
 * only paints the connecting segment.
 *
 * lineColor is a Q_PROPERTY so QML binds it to a Theme token rather than
 * reaching into the Theme singleton from C++.
 */
class CAVEWHERE_LIB_EXPORT cwMeasurementLineCanvas : public QCanvasPainterItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MeasurementLineCanvas)

    Q_PROPERTY(cwMeasurementInteraction* interaction READ interaction WRITE setInteraction NOTIFY interactionChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

    // Named lineColor to avoid QCanvasPainterItem's FINAL fillColor (canvas
    // background), which has a different meaning.
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged)

    // Color of the rubber-band when the cursor isn't over pickable geometry, so
    // the user sees that a click there won't produce a measurement.
    Q_PROPERTY(QColor invalidColor READ invalidColor WRITE setInvalidColor NOTIFY invalidColorChanged)

public:
    explicit cwMeasurementLineCanvas(QQuickItem* parent = nullptr);
    ~cwMeasurementLineCanvas() override;

    cwMeasurementInteraction* interaction() const { return m_interaction; }
    void setInteraction(cwMeasurementInteraction* interaction);

    cwCamera* camera() const { return m_camera; }
    void setCamera(cwCamera* camera);

    QColor lineColor() const { return m_lineColor; }
    void setLineColor(QColor color);

    QColor invalidColor() const { return m_invalidColor; }
    void setInvalidColor(QColor color);

protected:
    QCanvasPainterItemRenderer* createItemRenderer() const override;

signals:
    void interactionChanged();
    void cameraChanged();
    void lineColorChanged();
    void invalidColorChanged();

private:
    void connectInteraction(cwMeasurementInteraction* interaction);
    void connectCamera(cwCamera* camera);

    QPointer<cwMeasurementInteraction> m_interaction;
    QPointer<cwCamera> m_camera;

    // Opaque fallbacks so the strokes are well-defined before QML binds Theme
    // tokens (QCanvasPainter strokes become backend-defined otherwise).
    QColor m_lineColor = QColor(255, 255, 255);
    QColor m_invalidColor = QColor(255, 0, 0);
};

#endif // CWMEASUREMENTLINECANVAS_H
