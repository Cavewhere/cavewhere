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
#include "cwVisibilityProxy.h"
#include "cwLinePlotGeometry.h"
class cwRenderLinePlot;
class cwTrip;

/**
 * \brief The setVisible() target for one of a trip's line-plot keyword items.
 *
 * cwKeywordVisibility calls setVisible() on this object when keyword filters
 * change. It flips one contiguous vertex span in the render object's
 * per-vertex visibility buffer, exactly like cwRenderTexturedItemVisibility
 * does for scraps. The manager re-points the proxy (setTarget) after every
 * solve, because the vertex range shifts each time the geometry is rebuilt.
 *
 * A trip carries up to two of these over disjoint spans: the centerline item
 * (Type="Line Plot") owns the centerline sub-range and the splays item
 * (Type="Splays") owns the splay tail. Disjoint ranges keep each keyword
 * toggle independent, and both items inherit the trip's keywords so a
 * trip-level filter hides both spans.
 */
class CAVEWHERE_LIB_EXPORT cwLinePlotTripVisibility : public cwVisibilityProxy
{
    Q_OBJECT

public:
    cwLinePlotTripVisibility(cwTrip* trip,
                             QObject* parent = nullptr);

    cwTrip* trip() const { return m_trip; }

    // Re-binds the proxy to the render object and its current vertex span.
    void setTarget(cwRenderLinePlot* linePlot,
                   cwLinePlotGeometry::VertexRange range);

    // Re-applies the current keyword state to the render object; used after a
    // solve resets the visibility mask.
    void pushToTarget() { applyVisible(isVisible()); }

protected:
    void applyVisible(bool visible) override;

private:
    QPointer<cwRenderLinePlot> m_linePlot;
    QPointer<cwTrip> m_trip;
    cwLinePlotGeometry::VertexRange m_range;
};

#endif // CWLINEPLOTTRIPVISIBILITY_H
