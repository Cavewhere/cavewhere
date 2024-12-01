/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScrapOutlinePointView.h"
#include "cwScrap.h"
#include "cwScrapView.h"

cwScrapOutlinePointView::cwScrapOutlinePointView(QQuickItem *parent) :
    cwScrapPointView(parent)
{
}


void cwScrapOutlinePointView::setScrap(cwScrap *scrap)
{
    if(Scrap != scrap) {
        if(Scrap != nullptr) {
            disconnect(Scrap, &cwScrap::insertedPoints, this, &cwScrapOutlinePointView::pointsInserted);
            disconnect(Scrap, &cwScrap::removedPoints, this, &cwScrapOutlinePointView::pointsRemoved);
            disconnect(Scrap, &cwScrap::pointsReset, this, &cwScrapOutlinePointView::reset);
            disconnect(Scrap, &cwScrap::pointChanged, this, &cwScrapOutlinePointView::updateItemsPositions);
        }

        Scrap = scrap;

        if(Scrap != nullptr) {
            connect(Scrap, &cwScrap::insertedPoints, this, &cwScrapOutlinePointView::pointsInserted);
            connect(Scrap, &cwScrap::removedPoints, this, &cwScrapOutlinePointView::pointsRemoved);
            connect(Scrap, &cwScrap::pointsReset, this, &cwScrapOutlinePointView::reset);
            connect(Scrap, &cwScrap::pointChanged, this, &cwScrapOutlinePointView::updateItemsPositions);
        }

        reset();

        cwScrapPointView::setScrap(scrap);
    }
}

void cwScrapOutlinePointView::updateItemPosition(QQuickItem *item, int pointIndex)
{
    QPointF point = Scrap->points().at(pointIndex);
    QPointF imagePoint = cwScrapView::toImage(Scrap->parentNote()).map(point);
    qDebug() << "Set point:" << item << pointIndex << imagePoint;
    item->setPosition(imagePoint);

    // item->setProperty("position3D", QVector3D(point));
}


/**
 * @brief cwScrapControlPointView::reset
 *
 * Resets the view, this reloads all the points
 */
void cwScrapOutlinePointView::reset()
{
    if(Scrap) {
        //Update the number of points
        resizeNumberOfItems(Scrap->numberOfPoints());
    } else {
        //Remove all the points
        resizeNumberOfItems(0);
    }
}
