/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCALEBARITEM_H
#define CWSCALEBARITEM_H

//Qt includes
#include <QGraphicsObject>
#include <QFont>

//Our includes
#include "cwGlobals.h"
#include "cwUnits.h"

class CAVEWHERE_LIB_EXPORT cwScaleBarItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit cwScaleBarItem(QGraphicsItem* parent = nullptr);

    void setBorderRect(const QRectF& rect);
    void setScaleRatio(double ratio);

    //! The unit system the bar labels in — metric picks round m/km, imperial
    //! round ft/mi. Defaults to Metric.
    void setUnitSystem(cwUnits::UnitSystem system);
    cwUnits::UnitSystem unitSystem() const { return m_unitSystem; }

    double scaleRatio() const;
    void setLabelPointSize(double pointSize);

    double labelPointSize() const;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    //! The round scale length chosen for a given paper scale and unit system:
    //! its round \a value in \a unit and the paper \a widthInches it draws at
    //! (\a valid is false when nothing fits). Pure — the label math split out
    //! from the QGraphics geometry so it can be exercised directly.
    struct ScaleSelection {
        bool valid = false;
        double value = 0.0;
        cwUnits::LengthUnit unit = cwUnits::Meters;
        double widthInches = 0.0;
    };
    static ScaleSelection selectScale(double scaleRatio, double availableWidthInches,
                                      cwUnits::UnitSystem system);

private:
    void updateLayout();

    QRectF m_borderRect;
    double m_scaleRatio;

    QRectF m_boundingRect;
    QRectF m_barRect;
    QRectF m_labelRect;
    QString m_label;
    bool m_visible;
    double m_labelPointSize;
    QFont m_labelFont;
    cwUnits::UnitSystem m_unitSystem = cwUnits::Metric;
};

inline double cwScaleBarItem::scaleRatio() const
{
    return m_scaleRatio;
}

inline double cwScaleBarItem::labelPointSize() const
{
    return m_labelPointSize;
}

#endif // CWSCALEBARITEM_H
