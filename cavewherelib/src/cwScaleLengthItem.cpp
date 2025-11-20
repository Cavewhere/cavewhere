/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScaleLengthItem.h"
#include "cwTransformUpdater.h"
#include "cwSGLinesNode.h"

//Qt includes
#include <QDebug>
#include <QPainterPath>

cwScaleLengthItem::cwScaleLengthItem(QQuickItem *parent) :
    cwAbstract2PointItem(parent),
    LinesNode(nullptr)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

QSGNode *cwScaleLengthItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if(!oldNode) {
        oldNode = new QSGTransformNode();
        LinesNode = new cwSGLinesNode();

        oldNode->appendChildNode(LinesNode);
    }

    LinesNode->setLineWidth(cwSGLinesNode::lineWidthFromZoom(m_zoom, 2.0));
    LinesNode->setLines(Lines);

    return oldNode;
}

/**
    Updates the scale length path
  */
void cwScaleLengthItem::updateScaleLengthPath() {
    Lines = lengthLines();
    update();
}

/**
 * @brief cwScaleLengthItem::lengthLines
 * @return A vector with the 3 lines that make up the visual representation of the item
 */
QVector<QLineF> cwScaleLengthItem::lengthLines() const
{
    //Create the centerLine
    QLineF line0(p1(), p2());

    QPointF p1ViewportPosition = p1();
    QPointF p2ViewportPosition = p2();
    QLineF centerLine(p1ViewportPosition, p2ViewportPosition);

    double tailLength = cwSGLinesNode::lineWidthFromZoom(m_zoom, 8);
    QLineF perpendicularLine(QPointF(0, tailLength), QPointF(0, -tailLength));
    double rotation = 180.0 - centerLine.angle();

    QTransform transform;
    transform.translate(p1ViewportPosition.x(), p1ViewportPosition.y());
    transform.rotate(rotation);
    QLineF line1 = transform.map(perpendicularLine);

    line1.setP1(line1.p1());
    line1.setP2(line1.p2());

    transform = QTransform();
    transform.translate(p2ViewportPosition.x(), p2ViewportPosition.y());
    transform.rotate(rotation);
    QLineF line2 = transform.map(perpendicularLine);

    line2.setP1(line2.p1());
    line2.setP2(line2.p2());

    QVector<QLineF> lines;
    lines.reserve(3);
    lines.append(line0);
    lines.append(line1);
    lines.append(line2);
    return lines;
}
