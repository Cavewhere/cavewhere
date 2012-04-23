//Our includes
#include "cwNorthArrowItem.h"
#include "cwTransformUpdater.h"
#include "cwPositioner3D.h"


//Qt includes
#include <QPen>

cwNorthArrowItem::cwNorthArrowItem(QQuickItem *parent) :
    cwAbstract2PointItem(parent)
{
    NorthTextHandler = new cwPositioner3D(this);
    NorthTextHandler->setPosition3D(QVector3D(.1, .1, 0));
    NorthText = new QGraphicsTextItem(NorthTextHandler);

    NorthArrowLineHandler = new QQuickItem(this);
    NorthArrowLine = new QGraphicsPathItem(NorthArrowLineHandler);

    QFont font;
    font.setBold(true);
    font.setPointSize(14);

    NorthText->setPlainText("N");
    NorthText->setFont(font);
    NorthText->setPos(-NorthText->boundingRect().width() / 2.0, 0);

    NorthArrowLine->setPen(LinePen);
}

void cwNorthArrowItem::p1Updated() {
    NorthTextHandler->setPosition3D(QVector3D(P1));
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
void cwNorthArrowItem::disconnectTransformer()
{
    TransformUpdater->removePointItem(NorthTextHandler);
    TransformUpdater->removeTransformItem(NorthArrowLineHandler);
    disconnect(TransformUpdater, SIGNAL(updated()), this, SLOT(updateNorthArrowPath()));
}

/**
  Adds all items to the transformer and create a new connection to it
  */
void cwNorthArrowItem::connectTransformer() {
    TransformUpdater->addPointItem(NorthTextHandler);
    TransformUpdater->addTransformItem(NorthArrowLineHandler);
    connect(TransformUpdater, SIGNAL(updated()), SLOT(updateNorthArrowPath()));
}
