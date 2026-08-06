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

void cwLinePlotTripVisibility::setTarget(cwRenderLinePlot* linePlot, int vertexStart, int vertexCount)
{
    m_linePlot = linePlot;
    m_vertexStart = vertexStart;
    m_vertexCount = vertexCount;
}

void cwLinePlotTripVisibility::applyVisible(bool visible)
{
    if(m_linePlot) {
        m_linePlot->setRangeVisible(m_vertexStart, m_vertexCount, visible);
    }
}
