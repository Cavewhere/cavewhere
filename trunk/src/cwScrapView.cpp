//Our includes
#include "cwScrapView.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwScrapItem.h"

cwScrapView::cwScrapView(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Note(NULL),
    TransformUpdater(NULL)
{
}

/**
  Sets the note for the scrap view
  */
void cwScrapView::setNote(cwNote* note) {
    if(Note != note) {

        if(Note != NULL) {
            disconnect(this, NULL, Note, NULL);
        }

        Note = note;

        if(Note != NULL) {
            connect(Note, SIGNAL(scrapAdded()), SLOT(addScrapItem()));

            //This is full reset, update all the scraps
            updateAllScraps();
        }

        emit noteChanged();
    }
}

/**
  \brief Updates all the scrap items that are in the view
  */
void cwScrapView::addScrapItem() {
    if(Note == NULL) {
        return;
    }

    if(Note->scraps().empty()) {
        qDebug() << "This is a bug! There are no scraps" << LOCATION;
        return;
    }

    //make sure that there exactly one less scrap
    if(Note->scraps().size() - 1 != ScrapItems.size()) {
        qDebug() << "This is a bug! ItemLookup size mismatch" << LOCATION;
        return;
    }

    //Create a new scrap item
    cwScrapItem* scrapItem = new cwScrapItem(this);
    scrapItem->setScrap(Note->scraps().last());
    if(TransformUpdater != NULL) {
        TransformUpdater->addTransformItem(scrapItem);
    }
    ScrapItems.append(scrapItem);
}

///**
//  This should only be called by the scrap signals and slots

//    Else this function will do nothing.

//    This will update the polygon item's geometry
//  */
//void cwScrapView::updateScrapGeometry() {
//    cwScrap* scrap = qobject_cast<cwScrap*>(sender());
//    updateScrapGeometry(scrap);
//}

///**
//    This will update the polygon item's geometry
//  */
//void cwScrapView::updateScrapGeometry(cwScrap* scrap) {
//    if(scrap != NULL) {
//        QGraphicsPolygonItem* polygonItem = ItemLookup.value(scrap, NULL);
//        if(polygonItem != NULL) {
//            polygonItem->setPolygon(QPolygonF(scrap->points()));
//        }
//    }
//}

/**
  \brief Sets the transform updater
  */
void cwScrapView::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {
//        if(TransformUpdater != NULL) {
//            TransformUpdater->removeTransformItem(this);
//        }

        TransformUpdater = updater;

//        if(TransformUpdater != NULL) {
//            TransformUpdater->addTransformItem(this);
//        }

        updateAllScraps();

        emit transformUpdaterChanged();
    }
}

/**
  This should be called when the note object has changed.

  This will update all the scrap polygons
  */
void cwScrapView::updateAllScraps() {
    if(note() == NULL) { return; }

    int numberOfScraps = note()->scraps().size();

    if(ScrapItems.size() > numberOfScraps) {
        //Delete unused polygons
        for(int i = ScrapItems.size() - 1; i >= numberOfScraps; i--) {
            transformUpdater()->removeTransformItem(ScrapItems[i]);
            delete ScrapItems[i];
            ScrapItems.removeLast();
        }
    }

    if(ScrapItems.size() < numberOfScraps) {
        //Add new scrap items
        for(int i = ScrapItems.size(); i < numberOfScraps; i++) {
            ScrapItems.append(new cwScrapItem(this));
        }
    }

    Q_ASSERT(ScrapItems.size() == numberOfScraps);

    //Update
    for(int i = 0; i < numberOfScraps; i++) {
        cwScrap* scrap = note()->scrap(i);
        cwScrapItem* scrapItem = ScrapItems[i];
        scrapItem->setScrap(scrap);

        if(TransformUpdater != NULL) {
            transformUpdater()->addTransformItem(scrapItem);
        }
    }
}

///**
//  This creates an empty polygon
//  */
//QGraphicsPolygonItem* cwScrapView::createPolygon() {
//    //Create a new polygon item
//    QGraphicsPolygonItem* item = new QGraphicsPolygonItem(this);
//    item->setBrush(Qt::red);
//    item->setOpacity(0.25);
//    return item;
//}
