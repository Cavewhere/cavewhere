//Our includes
#include "cwScrapView.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwScrapItem.h"
#include "cwSelectionManager.h"

//Qt includes
#include <QQmlEngine>
#include <QSGTransformNode>

cwScrapView::cwScrapView(QQuickItem *parent) :
    QQuickItem(parent),
    Note(NULL),
    TransformUpdater(NULL),
    SelectionManager(new cwSelectionManager(this))
{
}

/**
  Sets the note for the scrap view
  */
void cwScrapView::setNote(cwNote* note) {
    if(Note != note) {

        if(Note != NULL) {
            disconnect(Note, NULL, this, NULL);
        }

        Note = note;

        if(Note != NULL) {
            connect(Note, SIGNAL(scrapAdded()), SLOT(addScrapItem()));
            connect(Note, &cwNote::removedScraps, this, &cwScrapView::updateRemovedScraps);
        }

        //This is full reset, update all the scraps
        updateAllScraps();

        emit noteChanged();
    }
}

/**
  \brief Updates all the scrap items that are in the view
  */
void cwScrapView:: addScrapItem() {
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
    QQmlContext* context = QQmlEngine::contextForObject(this);
    cwScrapItem* scrapItem = new cwScrapItem(context, this);
    scrapItem->setScrap(Note->scraps().last());
    scrapItem->setTransformUpdater(TransformUpdater);
    scrapItem->setSelectionManager(SelectionManager);
//    connect(scrapItem, SIGNAL(selectedChanged()), SLOT(updateSelection()), Qt::UniqueConnection);


    ScrapItems.append(scrapItem);

    //Select the scrapItem
    setSelectScrapIndex(ScrapItems.size() - 1);
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
  \brief Gets the scrap item at the index

  If the inde is out of bounds this return NULL
  */
cwScrapItem *cwScrapView::scrapItemAt(int index) {
    if(index >= 0 && index < ScrapItems.size()) {
        return ScrapItems[index];
    }
    return NULL;
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
    setSelectedScrapItem(items.first());
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
    int numberOfScraps;
    if(note() != NULL) {
        numberOfScraps = note()->scraps().size();
    } else {
        numberOfScraps = 0;
    }

    if(ScrapItems.size() > numberOfScraps) {
        //Delete unused polygons
        for(int i = ScrapItems.size() - 1; i >= numberOfScraps; i--) {
            if(ScrapItems[i] == selectedScrapItem()) {
                setSelectedScrapItem(NULL);
            }

            ScrapItems[i]->deleteLater();
            ScrapItems.removeLast();
        }
    }

    if(ScrapItems.size() < numberOfScraps) {
        QQmlContext* context = QQmlEngine::contextForObject(this);

        //Add new scrap items
        for(int i = ScrapItems.size(); i < numberOfScraps; i++) {
            cwScrapItem* item = new cwScrapItem(context, this);
            item->setTransformUpdater(TransformUpdater);
            ScrapItems.append(item);
        }
    }

    Q_ASSERT(ScrapItems.size() == numberOfScraps);

    //Update
    for(int i = 0; i < numberOfScraps; i++) {
        cwScrap* scrap = note()->scrap(i);
        cwScrapItem* scrapItem = ScrapItems[i];
        scrapItem->setScrap(scrap);
        scrapItem->setTransformUpdater(TransformUpdater);
        scrapItem->setSelectionManager(SelectionManager);
        connect(scrapItem, SIGNAL(selectedChanged()), SLOT(updateSelection()), Qt::UniqueConnection);
    }
}

/**
    Sets selectScrapIndex
*/
void cwScrapView::setSelectScrapIndex(int selectScrapIndex) {
    if(SelectScrapIndex != selectScrapIndex) {
        //If selection index is valid
        if(selectScrapIndex >= ScrapItems.size()) {
            qDebug() << "Can't select invalid scrap of index" << selectScrapIndex << LOCATION;
            return;
        }

        //Remove the selection from the previous
        cwScrapItem* oldScrapItem = selectedScrapItem();
        if(oldScrapItem != NULL) {
            oldScrapItem->setSelected(false);
        }

        if(selectScrapIndex >= 0) {
            cwScrapItem* newScrapItem = ScrapItems.at(selectScrapIndex);
            newScrapItem->setSelected(true);
        }

        SelectScrapIndex = selectScrapIndex;
        emit selectScrapIndexChanged();

        //Deselect any items that were selected in the scrap
        SelectionManager->clear();
    }
}

/**
    Gets selectedScrap
*/
cwScrapItem* cwScrapView::selectedScrapItem() const {
    if(selectScrapIndex() >= 0 && selectScrapIndex() < ScrapItems.size()) {
        //        Q_ASSERT(ScrapItems[selectScrapIndex()]->isSelected());
        return ScrapItems.at(selectScrapIndex());
    }
    return NULL;
}

/**
    Selects the scrap by pointer
  */
void cwScrapView::setSelectedScrapItem(cwScrapItem* scrapItem) {
    int index = ScrapItems.indexOf(scrapItem);
    setSelectScrapIndex(index);
}

/**
    This is a private slot

    Called when a scrapItem has changed there selection, this will update the selection
    model, to guaneteer that it is always up to date.
  */
void cwScrapView::updateSelection() {
    cwScrapItem* scrapItem = qobject_cast<cwScrapItem*>(sender());
    if(scrapItem != NULL) {
        setSelectedScrapItem(scrapItem);
    }
}

/**
 * @brief cwScrapView::updateRemovedScraps
 * @param begin - The index of the first scrap that was removed
 * @param end - The index of the last scrap that was removed
 */
void cwScrapView::updateRemovedScraps(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);
    updateAllScraps();
}
