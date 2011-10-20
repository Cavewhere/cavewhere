//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"

//Qt includes
#include <QGraphicsPolygonItem>

cwScrapItem::cwScrapItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Scrap(NULL),
    BorderItem(new QGraphicsPolygonItem(this))
{
    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9));
    BorderItem->setOpacity(0.25);
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


///**
//  Paints the scrap
//  */
//void cwScrapItem::paint(QPainter * painter, const QStyleOptionGraphicsItem *, QWidget *) {
//    if(Scrap == NULL) { return; }

//    QPolygonF scrapArea(Scrap->points());
//    painter->drawPolygon(scrapArea);
//}
