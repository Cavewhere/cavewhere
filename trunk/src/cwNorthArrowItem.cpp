//Our includes
#include "cwNorthArrowItem.h"
#include "cwTransformUpdater.h"
#include "cwPositioner3D.h"


//Qt includes
#include <QPen>

cwNorthArrowItem::cwNorthArrowItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{
    NorthTextHandler = new cwPositioner3D(this);
    NorthTextHandler->setPosition3D(QVector3D(.1, .1, 0));
    NorthText = new QGraphicsTextItem(NorthTextHandler);

    NorthArrowLineHandler = new QDeclarativeItem(this);
    NorthArrowLine = new QGraphicsPathItem(NorthArrowLineHandler);

    TransformUpdater = NULL;

    QFont font;
    font.setBold(true);
    font.setPointSize(14);

    NorthText->setPlainText("N");
    NorthText->setFont(font);
    NorthText->setPos(-NorthText->boundingRect().width() / 2.0, 0);

    QPen pen;
    pen.setWidthF(2.0);
    pen.setCosmetic(true);
    NorthArrowLine->setPen(pen);

    connect(this, SIGNAL(visibleChanged()), SLOT(updateTransformUpdater()));
}


/**
Sets transformUpdater, This will transform the north area objects so they're correct
*/
void cwNorthArrowItem::setTransformUpdater(cwTransformUpdater* transformUpdater) {
    if(TransformUpdater != transformUpdater) {
        if(TransformUpdater != NULL) {
            disconnectFromTransformer();
        }

        TransformUpdater = transformUpdater;

        updateTransformUpdater();

        emit transformUpdaterChanged();
    }
}

/**
Sets p1, The first point of the north up.  This is the bottom of the arrow, where
N will be located
*/
void cwNorthArrowItem::setP1(QPointF p1) {
    if(P1 != p1) {
        P1 = p1;

        NorthTextHandler->setPosition3D(QVector3D(P1));

        emit p1Changed();
    }
}

/**
Sets p2, The second point of the north up.  This is the top of the arrow
*/
void cwNorthArrowItem::setP2(QPointF p2) {
    if(P2 != p2) {
        P2 = p2;

        updateNorthArrowPath();

        emit p2Changed();
    }
}

/**
  Updates the transformUpdater
  */
void cwNorthArrowItem::updateTransformUpdater() {
    if(TransformUpdater != NULL) {
        if(isVisible()) {
            TransformUpdater->addPointItem(NorthTextHandler);
            TransformUpdater->addTransformItem(NorthArrowLineHandler);
            connect(TransformUpdater, SIGNAL(updated()), SLOT(updateNorthArrowPath()));
        } else {
            disconnectFromTransformer();
        }
    }
}

/**
  \brief This updates the north arrow geometry

  This is called when ever the north arrow path
  */
void cwNorthArrowItem::updateNorthArrowPath() {

    QPainterPath painterPath;
    painterPath.moveTo(p1());
    painterPath.lineTo(p2());

    //Calculate p3
    QPointF p1ViewportPosition = TransformUpdater->mapModelToViewport(p1());
    QPointF p2ViewportPosition = TransformUpdater->mapModelToViewport(p2());
    QLineF centerLine(p1ViewportPosition, p2ViewportPosition);

    QLineF arrowLine(QPointF(0.0, 0.0), QPointF(25.0, 10.0));
    QTransform transform;
    transform.translate(p2ViewportPosition.x(), p2ViewportPosition.y());
    transform.rotate(180.0 - centerLine.angle());
    arrowLine = transform.map(arrowLine);

    QVector3D p3 = TransformUpdater->mapFromViewportToModel(arrowLine.p2());

    painterPath.lineTo(p3.toPointF());
    NorthArrowLine->setPath(painterPath);
}

/**
  This removes all the items from the transformer
  */
void cwNorthArrowItem::disconnectFromTransformer()
{
    TransformUpdater->removePointItem(NorthTextHandler);
    TransformUpdater->removeTransformItem(NorthArrowLineHandler);
    disconnect(TransformUpdater, SIGNAL(updated()), this, SLOT(updateNorthArrowPath()));
}
