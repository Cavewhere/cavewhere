/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchViewState.h"

cwSketchViewState::cwSketchViewState(QObject *parent)
    : QObject(parent)
{
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

void cwSketchViewState::markInitialized()
{
    if (m_viewInitialized) {
        return;
    }
    m_viewInitialized = true;
    emit viewInitializedChanged();
}
