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

    if(transformUpdater()) {
        QSGTransformNode* transformNode = static_cast<QSGTransformNode*>(oldNode);
        transformNode->setMatrix(transformUpdater()->matrix());
    }


    LinesNode->setLines(Lines);

    return oldNode;
}

/**
    This is called when a new transformer is installed into this object.

    We need to connect it.
  */
void cwScaleLengthItem::connectTransformer() {
    connect(TransformUpdater, SIGNAL(updated()), SLOT(updateScaleLengthPath()));
}

/**
    This is called when a transformer is removed from this object

    We need to disconnect it
  */
void cwScaleLengthItem::disconnectTransformer() {
    disconnect(TransformUpdater, SIGNAL(updated()), this, SLOT(updateScaleLengthPath()));
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
    QPainterPath painterPath;
    QLineF line0(p1(), p2());

    QPointF p1ViewportPosition = TransformUpdater->mapModelToViewport(p1());
    QPointF p2ViewportPosition = TransformUpdater->mapModelToViewport(p2());
    QLineF centerLine(p1ViewportPosition, p2ViewportPosition);

    QLineF perpendicularLine(QPointF(0, 5), QPointF(0, -5));
    double rotation = 180.0 - centerLine.angle();

    QTransform transform;
    transform.translate(p1ViewportPosition.x(), p1ViewportPosition.y());
    transform.rotate(rotation);
    QLineF line1 = transform.map(perpendicularLine);

    line1.setP1(TransformUpdater->mapFromViewportToModel(line1.p1()).toPointF());
    line1.setP2(TransformUpdater->mapFromViewportToModel(line1.p2()).toPointF());

    transform = QTransform();
    transform.translate(p2ViewportPosition.x(), p2ViewportPosition.y());
    transform.rotate(rotation);
    QLineF line2 = transform.map(perpendicularLine);

    line2.setP1(TransformUpdater->mapFromViewportToModel(line2.p1()).toPointF());
    line2.setP2(TransformUpdater->mapFromViewportToModel(line2.p2()).toPointF());

    QVector<QLineF> lines;
    lines.reserve(3);
    lines.append(line0);
    lines.append(line1);
    lines.append(line2);
    return lines;
}
