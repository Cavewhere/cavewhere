/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMEASUREMENTLINERENDERER_H
#define CWMEASUREMENTLINERENDERER_H

//Qt includes
#include <QCanvasPainterItemRenderer>
#include <QColor>
#include <QPointF>

class cwMeasurementLineRenderer : public QCanvasPainterItemRenderer
{
public:
    cwMeasurementLineRenderer();
    ~cwMeasurementLineRenderer() override;

    void synchronize(QCanvasPainterItem* item) override;
    void paint(QCanvasPainter* painter) override;

private:
    QPointF m_firstScreen;
    QPointF m_endScreen;          //!< committed B, snapped hover, or raw cursor
    bool m_hasFirst = false;
    bool m_hasMeasurement = false;
    bool m_endValid = false;      //!< false when the end is a non-pickable cursor
    QColor m_lineColor;
    QColor m_invalidColor;
};

#endif // CWMEASUREMENTLINERENDERER_H
