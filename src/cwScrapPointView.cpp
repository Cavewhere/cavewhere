#include "cwScrapPointView.h"
#include "cwScrap.h"
#include "cwScrapItem.h"

cwScrapPointView::cwScrapPointView(QQuickItem *parent) :
    cwAbstractPointManager(parent),
    Scrap(NULL),
    ScrapItem(NULL)
{
}

/**
  Updates the station item's data
  */
void cwScrapPointView::updateItemData(QQuickItem* item, int pointIndex){
    Q_UNUSED(pointIndex);
    item->setProperty("scrap", QVariant::fromValue(scrap()));
    item->setProperty("scrapItem", QVariant::fromValue(scrapItem()));
}

/**
Sets scrapItem
*/
void cwScrapPointView::setScrapItem(cwScrapItem* scrapItem) {
    if(ScrapItem != scrapItem) {
        ScrapItem = scrapItem;
        updateAllItemData();
        emit scrapItemChanged();
    }
}

void cwScrapPointView::setScrap(cwScrap* scrap) {
    Q_UNUSED(scrap);
    emit scrapChanged();
}
