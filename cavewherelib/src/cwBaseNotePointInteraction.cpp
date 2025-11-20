/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBaseNotePointInteraction.h"
#include "cwScrapView.h"
#include "cwScrapItem.h"


cwBaseNotePointInteraction::cwBaseNotePointInteraction(QQuickItem *parent) :
    cwInteraction(parent)
{

}

cwBaseNotePointInteraction::~cwBaseNotePointInteraction()
{

}

/**
 Gets the scrap view for this interaction
*/
cwScrapView* cwBaseNotePointInteraction::scrapView() const {
    return ScrapView;
}

/**
    Sets the scrapView for the station interaction
*/
void cwBaseNotePointInteraction::setScrapView(cwScrapView* scrapView) {
    if(ScrapView != scrapView) {
        ScrapView = scrapView;
        emit scrapViewChanged();
    }
}

/**
 * @brief cwBaseNotePointInteraction::addPoint
 * @param notePosition
 */
void cwBaseNotePointInteraction::addPoint(QPointF notePosition)
{
    //Make sure we have a scrap view
    if(ScrapView == nullptr) {
        return;
    }

    //Find what scrap we need to add this station to
    QList<cwScrapItem*> scrapItems = ScrapView->scrapItemsAt(notePosition);

    //Select the scrap that we're going to add the station to
    cwScrapItem* scrapItem = selectScrapForAdding(scrapItems);

    //Make sure we have a scrap to add to
    if(scrapItem == nullptr) {
        //Do something to notify the user that they've clicked outside the bounds
        return;
    }

    cwScrap* scrap = scrapItem->scrap();

    //Make sure we have a scrap to add to
    if(scrap == nullptr) {
        //Do something to notify the user that they've clicked outside the bounds
        return;
    }

    //Call subclass
    addPoint(notePosition, scrapItem);
}

/**
    This selects the scrap for add new station should be added to

    It will also select the scrap item of where the stations will be added to

    This will return nullptr, if station can't be added
  */
cwScrapItem *cwBaseNotePointInteraction::selectScrapForAdding(QList<cwScrapItem *> scrapItems) {
    //Station isn't on a scrap
    if(scrapItems.isEmpty()) {
        return nullptr;
    }

    cwScrapItem* scrapItem = nullptr;
    if(scrapItems.size() == 1) {
        //Select the only scrap item
        scrapItem = scrapItems.first();
        ScrapView->setSelectScrapIndex(ScrapView->indexOf(scrapItem)); //Select the first scrap
    } else {
        //More than one scrap under the station

        //If the scrapview's select scrap
        cwScrapItem* selectedScrapItem = ScrapView->selectedScrapItem();
        if(scrapItems.contains(selectedScrapItem)) {
            //Use the currently select item's scrap
            scrapItem = selectedScrapItem;
        } else {
            //Just use the first scrap in the list
            scrapItem = scrapItems.first();
            ScrapView->setSelectScrapIndex(ScrapView->indexOf(scrapItem)); //Select the first scrap
        }
    }

    return scrapItem;
}

