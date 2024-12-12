/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWABSTRACT2POINTITEM_H
#define CWABSTRACT2POINTITEM_H

//Qt includes
#include <QQuickItem>
#include <QPen>

class cwAbstract2PointItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QPointF p1 READ p1 WRITE setP1 NOTIFY p1Changed)
    Q_PROPERTY(QPointF p2 READ p2 WRITE setP2 NOTIFY p2Changed)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged);

public:
    cwAbstract2PointItem(QQuickItem *parent);

    QPointF p1() const;
    void setP1(QPointF p1);

    QPointF p2() const;
    void setP2(QPointF p2);

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

protected:
    QPointF P1; //!< The first point in the north arrow
    QPointF P2; //!< The second point in the north arrow

    //For scaling the line correctly
    double m_zoom = 1.0;

    virtual void p1Updated() = 0;
    virtual void p2Updated() = 0;

signals:
    void p1Changed();
    void p2Changed();
    void zoomChanged();
};

/**
  Gets p1
  */
inline QPointF cwAbstract2PointItem::p1() const {
    return P1;
}

/**
  Gets p2
  */
inline QPointF cwAbstract2PointItem::p2() const {
    return P2;
}

#endif // CWABSTRACT2POINTITEM_H
