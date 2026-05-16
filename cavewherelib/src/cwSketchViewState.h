/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHVIEWSTATE_H
#define CWSKETCHVIEWSTATE_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwWorldToScreenMatrix.h"

// Runtime-only view state owned by cwSketch. Not serialized.
class CAVEWHERE_LIB_EXPORT cwSketchViewState : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchViewState)
    QML_UNCREATABLE("cwSketchViewState is owned by cwSketch")

    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(QPointF pan READ pan WRITE setPan NOTIFY panChanged)
    // False until the first setZoom/setPan, so callers can distinguish a fresh sketch from one panned back to (0,0).
    Q_PROPERTY(bool viewInitialized READ viewInitialized NOTIFY viewInitializedChanged)
    Q_PROPERTY(bool zoomLocked READ zoomLocked WRITE setZoomLocked NOTIFY zoomLockedChanged)
    // Toggled by the sketch canvas's debug overlay. cwScrapManager also
    // observes this: when false the diagnostic build (per-stroke polyline
    // copy + rejection list) is skipped entirely.
    Q_PROPERTY(bool debugOverlayVisible READ debugOverlayVisible WRITE setDebugOverlayVisible NOTIFY debugOverlayVisibleChanged)
    // Non-owning pointer set by SketchItem.qml so the view state can derive
    // screen/world scaling without duplicating the pixels-per-meter formula.
    Q_PROPERTY(cwWorldToScreenMatrix* worldToScreenMatrix READ worldToScreenMatrix WRITE setWorldToScreenMatrix NOTIFY worldToScreenMatrixChanged)

public:
    explicit cwSketchViewState(QObject *parent = nullptr);

    double  zoom() const { return m_zoom; }
    QPointF pan() const { return m_pan; }
    bool    viewInitialized() const { return m_viewInitialized; }
    bool    zoomLocked() const { return m_zoomLocked; }
    bool    debugOverlayVisible() const { return m_debugOverlayVisible; }
    cwWorldToScreenMatrix* worldToScreenMatrix() const { return m_worldToScreenMatrix; }

    void setZoom(double zoom);
    void setPan(QPointF pan);
    void setZoomLocked(bool zoomLocked);
    void setDebugOverlayVisible(bool visible);
    void setWorldToScreenMatrix(cwWorldToScreenMatrix *matrix);

    // Current on-screen pixels per world-meter, factoring in user zoom.
    // Returns 0 when the matrix is unset (tests without a view) so callers
    // can guard on a zero result.
    Q_INVOKABLE double pixelsPerMeter() const;

    // Convenience inverse: px / pixelsPerMeter(), returning 0 when the
    // matrix is unset. Use this any time you need a world-meter threshold
    // derived from a screen-pixel constant.
    Q_INVOKABLE double screenPixelsToWorldMeters(double px) const;

    // Pixels-per-meter for the *paper scale only* — i.e. excluding the
    // user-zoom multiplier. Use this when converting a length that itself
    // scales with user zoom (such as a stroke's rendered thickness, which
    // grows with zoom and so must produce a world-meter threshold that
    // also grows with zoom). Returns 0 when the matrix is unset.
    Q_INVOKABLE double pixelsPerMeterPaper() const;

signals:
    void zoomChanged();
    void panChanged();
    void viewInitializedChanged();
    void zoomLockedChanged();
    void debugOverlayVisibleChanged();
    void worldToScreenMatrixChanged();

private:
    double  m_zoom = 1.0;
    QPointF m_pan;
    bool    m_viewInitialized = false;
    bool    m_zoomLocked = false;
    bool    m_debugOverlayVisible = false;
    QPointer<cwWorldToScreenMatrix> m_worldToScreenMatrix;

    void markInitialized();
};

#endif // CWSKETCHVIEWSTATE_H
