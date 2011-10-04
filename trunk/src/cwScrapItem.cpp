//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"

//Qt includes
#include <QPainter>

cwScrapItem::cwScrapItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Scrap(NULL)
{
    setFlag(QGraphicsItem::ItemHasNoContents, false);
}

/**
  Sets the scrap that this item will visualize
  */
void cwScrapItem::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {
        Scrap = scrap;

        connect(Scrap, SIGNAL(insertedPoints(int,int)), SLOT(updateScrapItem()));
        connect(Scrap, SIGNAL(removedPoints(int,int)), SLOT(updateScrapItem()));

        emit scrapChanged();
    }
}

/**
  Paints the scrap
  */
void cwScrapItem::paint(QPainter * painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(Scrap == NULL) { return; }

    QPolygonF scrapArea(Scrap->points());
    painter->drawPolygon(scrapArea);
}
