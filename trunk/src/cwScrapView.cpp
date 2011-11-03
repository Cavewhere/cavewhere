//Our includes
#include "cwScrapView.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwScrapItem.h"

cwScrapView::cwScrapView(QDeclarativeItem *parent) :
    QDeclarativeItem(parent),
    Note(NULL),
    SelectedScrap(NULL),
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

    //Select the scrapItem

}

/**

This select the scrapItem, if the scrap item isn't null, and is part of the
  scrap view.  Otherwise this function does nothing
*/
void cwScrapView::setSelectedScrap(cwScrapItem* selectedScrap) {
    if(SelectedScrap != selectedScrap) {

        if(SelectedScrap != NULL) {
            //Deselect the scrap
            SelectedScrap->setSelected(false);
        }

        SelectedScrap = NULL;

        if(selectedScrap != NULL) {
            if(ScrapItems.contains(selectedScrap)) {

                SelectedScrap = selectedScrap;

                if(SelectedScrap != NULL) {
                    //Select the new selectedScrap
                    SelectedScrap->setSelected(true);
                }
            }
        }

        emit selectedScrapChanged();
    }
}

/**
    Gets the scrapItem at notePoint

    If no scrap is defined at this point NULL is returned.

    \param notePoint - Normalized note coordinate
  */
QList<cwScrapItem*> cwScrapView::scrapItemsAt(QPointF notePoint) {
    QList<cwScrapItem*> items;
    foreach(cwScrapItem* scrapItem, ScrapItems) {
        QPolygonF polygon(scrapItem->scrap()->points());
        if(polygon.containsPoint(notePoint, Qt::OddEvenFill)) {
            items.append(scrapItem);
        }
    }

    return items;
}

/**
  \brief Selects the scrap at the notePoint
  */
void cwScrapView::selectScrapAt(QPointF notePoint) {
    QList<cwScrapItem*> items = scrapItemsAt(notePoint);
    if(items.isEmpty()) {
        return; //Nothing to select
    }

    //Select the first item
    setSelectedScrap(items.first());
}

/**
  \brief Sets the transform updater
  */
void cwScrapView::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {
        TransformUpdater = updater;

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

            if(ScrapItems[i] == SelectedScrap) {
                setSelectedScrap(NULL);
            }

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
