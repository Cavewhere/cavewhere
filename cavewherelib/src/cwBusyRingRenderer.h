/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBUSYRINGRENDERER_H
#define CWBUSYRINGRENDERER_H

//Qt includes
#include <QCanvasPainterItemRenderer>
#include <QColor>

class cwBusyRingRenderer : public QCanvasPainterItemRenderer
{
public:
    cwBusyRingRenderer();
    ~cwBusyRingRenderer() override;

    void synchronizeData(QCanvasPainterItem* item) override;
    void paint(QCanvasPainter* painter) override;

private:
    QColor m_startColor;
    QColor m_midColor;
    QColor m_endColor;
    QColor m_leadColor;
    double m_lineWidthRatio = 0.0;
};

#endif // CWBUSYRINGRENDERER_H
