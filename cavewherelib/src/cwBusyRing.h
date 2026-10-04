/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBUSYRING_H
#define CWBUSYRING_H

//Qt includes
#include <QCanvasPainterItem>
#include <QColor>
#include <QQmlEngine>

//Our includes
#include "CaveWhereLibExport.h"

/**
 * The style's busy mark: one open ring whose stroke sweeps from transparent
 * through the four progress colors to a bright leading edge at twelve o'clock.
 *
 * The item paints a still ring. BusyIndicator.qml spins it by rotating the
 * item, so a turn costs no repaint.
 */
class CAVEWHERE_LIB_EXPORT cwBusyRing : public QCanvasPainterItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BusyRing)

    Q_PROPERTY(QColor startColor READ startColor WRITE setStartColor NOTIFY startColorChanged)
    Q_PROPERTY(QColor midColor READ midColor WRITE setMidColor NOTIFY midColorChanged)
    Q_PROPERTY(QColor endColor READ endColor WRITE setEndColor NOTIFY endColorChanged)
    Q_PROPERTY(QColor leadColor READ leadColor WRITE setLeadColor NOTIFY leadColorChanged)

    //! Stroke width as a share of the item's shorter side
    Q_PROPERTY(double lineWidthRatio READ lineWidthRatio WRITE setLineWidthRatio NOTIFY lineWidthRatioChanged)

public:
    explicit cwBusyRing(QQuickItem* parent = nullptr);
    ~cwBusyRing() override;

    QColor startColor() const { return m_startColor; }
    void setStartColor(QColor color);

    QColor midColor() const { return m_midColor; }
    void setMidColor(QColor color);

    QColor endColor() const { return m_endColor; }
    void setEndColor(QColor color);

    QColor leadColor() const { return m_leadColor; }
    void setLeadColor(QColor color);

    double lineWidthRatio() const { return m_lineWidthRatio; }
    void setLineWidthRatio(double ratio);

protected:
    QCanvasPainterItemRenderer* createItemRenderer() const override;

signals:
    void startColorChanged();
    void midColorChanged();
    void endColorChanged();
    void leadColorChanged();
    void lineWidthRatioChanged();

private:
    //Opaque fallbacks so the stroke is well-defined before QML binds Theme
    //tokens (QCanvasPainter strokes become backend-defined otherwise).
    QColor m_startColor = Qt::darkCyan;
    QColor m_midColor = Qt::cyan;
    QColor m_endColor = Qt::blue;
    QColor m_leadColor = Qt::white;
    double m_lineWidthRatio;
};

#endif // CWBUSYRING_H
