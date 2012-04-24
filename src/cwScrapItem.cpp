//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwScrapStationView.h"

//Qt includes
#include <QGraphicsPolygonItem>
#include <QPen>
#include <QQmlEngine>

cwScrapItem::cwScrapItem(QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    BorderItemHandler(new QQuickItem(this)),
    //FIXME: Port BorderItem to qt5
//    BorderItem(new QGraphicsPolygonItem(BorderItemHandler)),
    StationView(new cwScrapStationView(this))
{
    //Set the declarative context for the station view
    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQmlEngine::setContextForObject(StationView, context);

//    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
}

cwScrapItem::cwScrapItem(QQmlContext *context, QQuickItem *parent) :
    QQuickItem(parent),
    Scrap(NULL),
    TransformUpdater(NULL),
    BorderItemHandler(new QQuickItem(this)),
        //FIXME: Port BorderItem to qt5
//    BorderItem(new QGraphicsPolygonItem(BorderItemHandler)),
    StationView(new cwScrapStationView(this))
{
    StationView->setScrapItem(this);

    //Set the declarative context for the station view
    QQmlEngine::setContextForObject(this, context);
    QQmlEngine::setContextForObject(StationView, context);

//    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
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
    //FIXME: Add set polygon for border item
//    BorderItem->setPolygon(QPolygonF(Scrap->points()));
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
        //FIXME: Fix border item's pen
//        BorderItem->setPen(pen);

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

