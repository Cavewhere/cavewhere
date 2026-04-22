/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchViewState.h"
#include "cwWorldToScreenMatrix.h"

cwSketchViewState::cwSketchViewState(QObject *parent)
    : QObject(parent)
{
}

void cwSketchViewState::setWorldToScreenMatrix(cwWorldToScreenMatrix *matrix)
{
    if (m_worldToScreenMatrix == matrix) {
        return;
    }
    m_worldToScreenMatrix = matrix;
    emit worldToScreenMatrixChanged();
}

double cwSketchViewState::pixelsPerMeter() const
{
    if (!m_worldToScreenMatrix) {
        return 0.0;
    }
    // m11 is the positive world-meter→screen-pixel scale baked into the
    // matrix; the Y-axis flip lives in m22. Multiplying by zoom mirrors
    // the _pxPerMeter derivation in SketchItem.qml.
    return static_cast<double>(m_worldToScreenMatrix->matrix()(0, 0)) * m_zoom;
}

double cwSketchViewState::screenPixelsToWorldMeters(double px) const
{
    const double k = pixelsPerMeter();
    if (k <= 0.0) {
        return 0.0;
    }
    return px / k;
}

double cwSketchViewState::pixelsPerMeterPaper() const
{
    if (!m_worldToScreenMatrix) {
        return 0.0;
    }
    return static_cast<double>(m_worldToScreenMatrix->matrix()(0, 0));
}

void cwSketchViewState::setZoom(double zoom)
{
    markInitialized();
    if (m_zoom == zoom) {
        return;
    }
    m_zoom = zoom;
    emit zoomChanged();
}

void cwSketchViewState::setPan(QPointF pan)
{
    markInitialized();
    if (m_pan == pan) {
        return;
    }
    m_pan = pan;
    emit panChanged();
}

void cwSketchViewState::setZoomLocked(bool zoomLocked)
{
    if (m_zoomLocked == zoomLocked) {
        return;
    }
    m_zoomLocked = zoomLocked;
    emit zoomLockedChanged();
}

void cwSketchViewState::setDebugOverlayVisible(bool visible)
{
    if (m_debugOverlayVisible == visible) {
        return;
    }
    m_debugOverlayVisible = visible;
    emit debugOverlayVisibleChanged();
}

void cwSketchViewState::markInitialized()
{
    if (m_viewInitialized) {
        return;
    }
    m_viewInitialized = true;
    emit viewInitializedChanged();
}
