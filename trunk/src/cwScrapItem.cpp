//Our includes
#include "cwScrapItem.h"
#include "cwScrap.h"

//Qt includes
#include <QGraphicsPolygonItem>
#include <QPen>

cwScrapItem::cwScrapItem(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Scrap(NULL),
    BorderItem(new QGraphicsPolygonItem(this))
{
    BorderItem->setBrush(QColor(0x20, 0x8b, 0xe9, 50));
    setSelected(false);
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

        emit selectedChanged();
    }
}
