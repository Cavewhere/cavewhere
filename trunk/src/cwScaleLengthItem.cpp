//Our includes
#include "cwScaleLengthItem.h"
#include "cwTransformUpdater.h"

//Qt includes
#include <QDebug>

cwScaleLengthItem::cwScaleLengthItem(QDeclarativeItem *parent) :
    cwAbstract2PointItem(parent)
{
    LengthHandler = new QDeclarativeItem(this);
    LengthLine = new QGraphicsPathItem(LengthHandler);
    LengthLine->setPen(LinePen);
}

/**
    This is called when a new transformer is installed into this object.

    We need to connect it.
  */
void cwScaleLengthItem::connectTransformer() {
    TransformUpdater->addTransformItem(LengthHandler);
    connect(TransformUpdater, SIGNAL(updated()), SLOT(updateScaleLengthPath()));
}

/**
    This is called when a transformer is removed from this object

    We need to disconnect it
  */
void cwScaleLengthItem::disconnectTransformer() {
    TransformUpdater->addTransformItem(LengthHandler);
    disconnect(TransformUpdater, SIGNAL(updated()), this, SLOT(updateScaleLengthPath()));
}

/**
    Updates the scale length path
  */
void cwScaleLengthItem::updateScaleLengthPath() {
    //Create the centerLine
    QPainterPath painterPath;
    painterPath.moveTo(p1());
    painterPath.lineTo(p2());

    QPointF p1ViewportPosition = TransformUpdater->mapModelToViewport(p1());
    QPointF p2ViewportPosition = TransformUpdater->mapModelToViewport(p2());
    QLineF centerLine(p1ViewportPosition, p2ViewportPosition);

    QLineF perpendicularLine(QPointF(0, 5), QPointF(0, -5));
    double rotation = 180.0 - centerLine.angle();

    QTransform transform;
    transform.translate(p1ViewportPosition.x(), p1ViewportPosition.y());
    transform.rotate(rotation);
    QLineF line1 = transform.map(perpendicularLine);

    painterPath.moveTo(TransformUpdater->mapFromViewportToModel(line1.p1()).toPointF());
    painterPath.lineTo(TransformUpdater->mapFromViewportToModel(line1.p2()).toPointF());

    transform = QTransform();
    transform.translate(p2ViewportPosition.x(), p2ViewportPosition.y());
    transform.rotate(rotation);
    QLineF line2 = transform.map(perpendicularLine);

    painterPath.moveTo(TransformUpdater->mapFromViewportToModel(line2.p1()).toPointF());
    painterPath.lineTo(TransformUpdater->mapFromViewportToModel(line2.p2()).toPointF());

    LengthLine->setPath(painterPath);
}
