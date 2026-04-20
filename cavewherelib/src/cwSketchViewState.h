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
#include <QPointF>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

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

public:
    explicit cwSketchViewState(QObject *parent = nullptr);

    double  zoom() const { return m_zoom; }
    QPointF pan() const { return m_pan; }
    bool    viewInitialized() const { return m_viewInitialized; }
    bool    zoomLocked() const { return m_zoomLocked; }

    void setZoom(double zoom);
    void setPan(QPointF pan);
    void setZoomLocked(bool zoomLocked);

signals:
    void zoomChanged();
    void panChanged();
    void viewInitializedChanged();
    void zoomLockedChanged();

private:
    double  m_zoom = 1.0;
    QPointF m_pan;
    bool    m_viewInitialized = false;
    bool    m_zoomLocked = true;

    void markInitialized();
};

#endif // CWSKETCHVIEWSTATE_H
