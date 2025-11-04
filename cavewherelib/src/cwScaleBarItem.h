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

class CAVEWHERE_LIB_EXPORT cwScaleBarItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit cwScaleBarItem(QGraphicsItem* parent = nullptr);

    void setBorderRect(const QRectF& rect);
    void setScaleRatio(double ratio);

    double scaleRatio() const;
    void setLabelPointSize(double pointSize);

    double labelPointSize() const;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

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
