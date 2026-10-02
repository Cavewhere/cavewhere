/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROGRESSRINGRENDERER_H
#define CWPROGRESSRINGRENDERER_H

//Qt includes
#include <QCanvasPainterItemRenderer>
#include <QColor>

class cwProgressRingRenderer : public QCanvasPainterItemRenderer
{
public:
    cwProgressRingRenderer();
    ~cwProgressRingRenderer() override;

    void synchronizeData(QCanvasPainterItem* item) override;
    void paint(QCanvasPainter* painter) override;

private:
    double m_progress = -1.0;
    double m_spinAngle = 0.0;
    QColor m_trackColor;
    QColor m_arcColor;
};

#endif // CWPROGRESSRINGRENDERER_H
