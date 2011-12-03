//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwScrapStationView.h"

//Qt includes
#include <QGraphicsPolygonItem>
#include <QPen>
#include <QDeclarativeEngine>

cwScrapItem::cwScrapItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    BorderItemHandler(new QDeclarativeItem(this)),
    BorderItem(new QGraphicsPolygonItem(BorderItemHandler)),
    StationView(new cwScrapStationView(this))
{
    //Set the declarative context for the station view
    QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
    QDeclarativeEngine::setContextForObject(StationView, context);

    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
}

cwScrapItem::cwScrapItem(QDeclarativeContext *context, QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    BorderItemHandler(new QDeclarativeItem(this)),
    BorderItem(new QGraphicsPolygonItem(BorderItemHandler)),
    StationView(new cwScrapStationView(this))
{
    StationView->setScrapItem(this);

    //Set the declarative context for the station view
    QDeclarativeEngine::setContextForObject(this, context);
    QDeclarativeEngine::setContextForObject(StationView, context);

    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
}

cwScrapItem::~cwScrapItem()
{
    if(TransformUpdater != NULL) {
        TransformUpdater->removeTransformItem(BorderItemHandler);
    }
}

/**
  Sets the scrap that this item will visualize
  */
void cwScrapItem::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {
        if(Scrap != NULL) {
            disconnect(Scrap, NULL, this, NULL);
        }

        Scrap = scrap;

        StationView->setScrap(Scrap);

        if(Scrap != NULL) {
            connect(Scrap, SIGNAL(insertedPoints(int,int)), SLOT(updateScrapGeometry()));
            connect(Scrap, SIGNAL(removedPoints(int,int)), SLOT(updateScrapGeometry()));
            updateScrapGeometry();
        }

        emit scrapChanged();
    }
}

/**
    This will update the polygon item's geometry
  */
void cwScrapItem::updateScrapGeometry() {
    BorderItem->setPolygon(QPolygonF(Scrap->points()));
}

/**
Sets the scrap item as the selected scrap
*/
void cwScrapItem::setSelected(bool selected) {
    if(Selected != selected) {
        Selected = selected;

        //Make the pen wider
        QPen pen;
        pen.setCosmetic(true);
        if(Selected) {
            pen.setWidth(2.0);
        } else {
            pen.setWidth(1.0);
        }
        BorderItem->setPen(pen);

        if(!selected) {
            StationView->clearSelection();
        }

        emit selectedChanged();
    }
}


/**
Sets transformUpdater
*/
void cwScrapItem::setTransformUpdater(cwTransformUpdater* transformUpdater) {
    if(TransformUpdater != transformUpdater) {
        if(TransformUpdater != NULL) {
            TransformUpdater->removeTransformItem(BorderItemHandler);
        }

        TransformUpdater = transformUpdater;
        StationView->setTransformUpdater(TransformUpdater);

        if(TransformUpdater != NULL) {
            TransformUpdater->addTransformItem(BorderItemHandler);
        }

        emit transformUpdaterChanged();
    }
}

