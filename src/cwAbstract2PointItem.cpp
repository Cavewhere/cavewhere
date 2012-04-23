#include "cwAbstract2PointItem.h"

cwAbstract2PointItem::cwAbstract2PointItem(QQuickItem *parent) :
    QQuickItem(parent)
{
    TransformUpdater = NULL;
    connect(this, SIGNAL(visibleChanged()), SLOT(updateTransformUpdater()));

    QPen pen;
    pen.setWidthF(2.0);
    pen.setCosmetic(true);
    LinePen = pen;
}

/**
Sets transformUpdater, This will transform the north area objects so they're correct
*/
void cwAbstract2PointItem::setTransformUpdater(cwTransformUpdater* transformUpdater) {
    if(TransformUpdater != transformUpdater) {
        if(TransformUpdater != NULL) {
            disconnectTransformer();
        }

        TransformUpdater = transformUpdater;

        updateTransformUpdater();

        emit transformUpdaterChanged();
    }
}

/**
  Updates the transformUpdater
  */
void cwAbstract2PointItem::updateTransformUpdater() {
    if(TransformUpdater != NULL) {
        if(isVisible()) {
            connectTransformer();
        } else {
            disconnectTransformer();
        }
    }
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
