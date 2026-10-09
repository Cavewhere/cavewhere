/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLinePlotTripVisibility.h"
#include "cwRenderLinePlot.h"
#include "cwTrip.h"

cwLinePlotTripVisibility::cwLinePlotTripVisibility(cwTrip* trip,
                                                   QObject* parent)
    : cwVisibilityProxy(parent),
      m_trip(trip)
{
}

void cwLinePlotTripVisibility::setTarget(cwRenderLinePlot* linePlot,
                                         cwLinePlotGeometry::VertexRange range)
{
    m_linePlot = linePlot;
    m_range = range;
}

void cwLinePlotTripVisibility::applyVisible(bool visible)
{
    if(m_linePlot) {
        m_linePlot->setRangeVisible(m_range.start, m_range.count, visible);
    }
}

void cwLinePlotTripVisibility::applyColorValue(float value)
{
    if(m_linePlot) {
        m_linePlot->setRangeColorValue(m_range.start, m_range.count, value);
    }
}

void cwLinePlotTripVisibility::pushToTarget()
{
    // A solve resets the span to visible, so only a hidden span has a state to
    // publish. The color value always goes out: setRangeColorValue returns early
    // when the span already holds it.
    if(!isVisible()) {
        applyVisible(false);
    }
    applyColorValue(colorValue());
}
