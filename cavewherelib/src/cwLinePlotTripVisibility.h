/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTTRIPVISIBILITY_H
#define CWLINEPLOTTRIPVISIBILITY_H

//Qt includes
#include <QObject>
#include <QPointer>

//Our includes
#include "cwGlobals.h"
class cwRenderLinePlot;
class cwTrip;

/**
 * \brief The setVisible() target for a trip's centerline keyword item.
 *
 * cwKeywordVisibility calls setVisible() on this object when keyword filters
 * change. It flips the trip's contiguous span in the render object's per-vertex
 * visibility buffer, exactly like cwRenderTexturedItemVisibility does for
 * scraps. The manager re-points the proxy (setTarget) after every solve,
 * because the vertex range shifts each time the geometry is rebuilt.
 */
class CAVEWHERE_LIB_EXPORT cwLinePlotTripVisibility : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

public:
    cwLinePlotTripVisibility(cwTrip* trip,
                             QObject* parent = nullptr);

    bool isVisible() const { return m_visible; }
    cwTrip* trip() const { return m_trip; }

    // Re-binds the proxy to the render object and the trip's current vertex span.
    void setTarget(cwRenderLinePlot* linePlot, int vertexStart, int vertexCount);

public slots:
    void setVisible(bool visible);

signals:
    void visibleChanged();

private:
    QPointer<cwRenderLinePlot> m_linePlot;
    QPointer<cwTrip> m_trip;
    int m_vertexStart = 0;
    int m_vertexCount = 0;
    bool m_visible = true;
};

#endif // CWLINEPLOTTRIPVISIBILITY_H
