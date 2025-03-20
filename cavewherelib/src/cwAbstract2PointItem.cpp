/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwAbstract2PointItem.h"

cwAbstract2PointItem::cwAbstract2PointItem(QQuickItem *parent) :
    QQuickItem(parent)
{

}

/**
Sets p1, The first point
*/
void cwAbstract2PointItem::setP1(QPointF p1) {
    if(P1 != p1) {
        P1 = p1;

        p1Updated();

        emit p1Changed();
    }
}

/**
Sets p2, The second point
*/
void cwAbstract2PointItem::setP2(QPointF p2) {
    if(P2 != p2) {
        P2 = p2;

        p2Updated();

        emit p2Changed();
    }
}

void cwAbstract2PointItem::setZoom(double zoom)
{
    if(m_zoom != zoom) {
        m_zoom = zoom;
        // Lines = lengthLines();
        update();
        emit zoomChanged();
    }
}
