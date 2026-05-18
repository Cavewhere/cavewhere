/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZCLIPPOLYGONCANVAS_H
#define CWLAZCLIPPOLYGONCANVAS_H

//Qt includes
#include <QCanvasPainterItem>
#include <QColor>
#include <QPointF>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

class cwLazClipInteraction;

/**
 * Overlay item that paints the in-progress clip polygon: filled outline
 * when closed, polyline + rubber-band segment while drawing, vertex dots,
 * and a snap-target highlight on the first vertex when the hover point is
 * within snap range.
 *
 * Projection happens in the renderer's synchronize() — GUI thread is
 * blocked there, so we can safely call cwLazClipInteraction::worldToScreen
 * (which reads cwCamera matrices) to convert world-XY vertices into the
 * canvas's local pixel space.
 *
 * Colors are exposed as Q_PROPERTY so QML can bind them to Theme tokens
 * (we don't reach into the QML Theme singleton from C++).
 */
class CAVEWHERE_LIB_EXPORT cwLazClipPolygonCanvas : public QCanvasPainterItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LazClipPolygonCanvas)

    Q_PROPERTY(cwLazClipInteraction* interaction READ interaction WRITE setInteraction NOTIFY interactionChanged)
    Q_PROPERTY(QPointF hoverScreenPos READ hoverScreenPos WRITE setHoverScreenPos NOTIFY hoverScreenPosChanged)
    Q_PROPERTY(bool snapActive READ snapActive WRITE setSnapActive NOTIFY snapActiveChanged)

    // Names avoid QCanvasPainterItem's `fillColor` (canvas background, FINAL)
    // — that property has a different meaning and we don't want to shadow it.
    Q_PROPERTY(QColor polygonOutlineColor READ polygonOutlineColor WRITE setPolygonOutlineColor NOTIFY polygonOutlineColorChanged)
    Q_PROPERTY(QColor polygonFillColor READ polygonFillColor WRITE setPolygonFillColor NOTIFY polygonFillColorChanged)
    Q_PROPERTY(QColor polygonVertexBorderColor READ polygonVertexBorderColor WRITE setPolygonVertexBorderColor NOTIFY polygonVertexBorderColorChanged)

public:
    explicit cwLazClipPolygonCanvas(QQuickItem* parent = nullptr);
    ~cwLazClipPolygonCanvas() override;

    cwLazClipInteraction* interaction() const { return m_interaction; }
    void setInteraction(cwLazClipInteraction* interaction);

    QPointF hoverScreenPos() const { return m_hoverScreenPos; }
    void setHoverScreenPos(QPointF p);

    bool snapActive() const { return m_snapActive; }
    void setSnapActive(bool active);

    QColor polygonOutlineColor() const { return m_outlineColor; }
    void setPolygonOutlineColor(QColor c);

    QColor polygonFillColor() const { return m_fillColor; }
    void setPolygonFillColor(QColor c);

    QColor polygonVertexBorderColor() const { return m_vertexBorderColor; }
    void setPolygonVertexBorderColor(QColor c);

protected:
    QCanvasPainterItemRenderer* createItemRenderer() const override;

signals:
    void interactionChanged();
    void hoverScreenPosChanged();
    void snapActiveChanged();
    void polygonOutlineColorChanged();
    void polygonFillColorChanged();
    void polygonVertexBorderColorChanged();

private slots:
    void onInteractionPolygonChanged();
    void onInteractionStateChanged();

private:
    void connectInteraction(cwLazClipInteraction* interaction);

    QPointer<cwLazClipInteraction> m_interaction;
    QPointF m_hoverScreenPos;
    bool m_snapActive = false;

    // Opaque fallbacks so the renderer always has valid colors before QML
    // binds Theme tokens — otherwise QCanvasPainter strokes/fills become
    // backend-defined (often black or transparent).
    QColor m_outlineColor = QColor(255, 255, 255);
    QColor m_fillColor = QColor(255, 255, 255, 46);
    QColor m_vertexBorderColor = QColor(0, 0, 0);
};

#endif // CWLAZCLIPPOLYGONCANVAS_H
