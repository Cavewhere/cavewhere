/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwNorthArrowItem.h"
#include "cwTransformUpdater.h"
//#include "cwBasePositioner.h"
#include "cwSGLinesNode.h"


//Qt includes
#include <QPen>
#include <QPainterPath>

cwNorthArrowItem::cwNorthArrowItem(QQuickItem *parent) :
    cwAbstract2PointItem(parent),
    NorthArrowLinesNode(nullptr)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

/**
 * @brief cwNorthArrowItem::updatePaintNode
 * @param oldNode
 * @return The oldNode
 *
 * This is run in the rendering thread and defines the geometry for the object
 */
QSGNode *cwNorthArrowItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if(!oldNode) {
        oldNode = new QSGTransformNode();
        NorthArrowLinesNode = new cwSGLinesNode();

        oldNode->appendChildNode(NorthArrowLinesNode);
    }

    NorthArrowLinesNode->setLineWidth(cwSGLinesNode::lineWidthFromZoom(m_zoom, 2.0));
    NorthArrowLinesNode->setLineStrip(NorthArrowLineStrip);

    return oldNode;
}

void cwNorthArrowItem::p1Updated() {
    updateNorthArrowPath();
}

/**
  \brief This updates the north arrow geometry

  This is called when ever the north arrow path
  */
void cwNorthArrowItem::updateNorthArrowPath() {
    NorthArrowLineStrip = createNorthArrowLineStrip();
    update();
}

/**
 * @brief cwNorthArrowItem::createNorthArrowLines
 * @return Returns the geometry for the north arrow lines, in a line strip
 */
QVector<QPointF> cwNorthArrowItem::createNorthArrowLineStrip() const
{
    //Calculate p3
    QPointF p1ViewportPosition = p1();
    QPointF p2ViewportPosition = p2();
    QLineF centerLine(p1ViewportPosition, p2ViewportPosition);

    QLineF arrowLine1(QPointF(0.0, 0.0), QPointF(20.0, 10.0));
    QLineF arrowLine2(QPointF(0.0, 0.0), QPointF(20.0, -10.0));
    QTransform transform;
    transform.translate(p2ViewportPosition.x(), p2ViewportPosition.y());
    transform.rotate(180.0 - centerLine.angle());
    arrowLine1 = transform.map(arrowLine1);
    arrowLine2 = transform.map(arrowLine2);

    QPointF p3 = arrowLine1.p2();
    QPointF p4 = arrowLine2.p2();

    QVector<QPointF> lineStrip;
    lineStrip.reserve(3);
    lineStrip.append(p1());
    lineStrip.append(p2());
    lineStrip.append(p3);
    lineStrip.append(p2());
    lineStrip.append(p4);

    return lineStrip;
}
