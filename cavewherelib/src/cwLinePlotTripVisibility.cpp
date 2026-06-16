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
    : QObject(parent),
      m_trip(trip)
{
}

void cwLinePlotTripVisibility::setTarget(cwRenderLinePlot* linePlot, int tripId)
{
    m_linePlot = linePlot;
    m_tripId = tripId;
}

void cwLinePlotTripVisibility::setVisible(bool visible)
{
    if(m_visible == visible) {
        return;
    }

    m_visible = visible;
    if(m_linePlot) {
        m_linePlot->setTripVisible(m_tripId, visible);
    }

    emit visibleChanged();
}
