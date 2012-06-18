//Our includes
#include "cwAbstractPointManager.h"
#include "cwTransformUpdater.h"
#include "cwDebug.h"

//Qt includes
#include <QQmlEngine>
#include <QQmlContext>

cwAbstractPointManager::cwAbstractPointManager(QQuickItem *parent) :
    QQuickItem(parent),
    TransformUpdater(NULL),
    ItemComponent(NULL),
    SelectedItemIndex(-1)
{

}

/**
  \brief Sets the transform updater

  This will transform all the station in this station view
  */
void cwAbstractPointManager::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {
        if(TransformUpdater != NULL) {
            //Remove all previous stations
            foreach(QQuickItem* item, Items) {
                TransformUpdater->removePointItem(item);
            }
        }

        TransformUpdater = updater;

        if(TransformUpdater != NULL) {
            //Add station to the new transformUpdater
            foreach(QQuickItem* item, Items) {
                TransformUpdater->addPointItem(item);
            }
        }

        emit transformUpdaterChanged();
        update();
    }
}

/**
  This creates the station component used generate the station symobols
  */
void cwAbstractPointManager::createComponent() {
    //Make sure we have a note component so we can create it
    if(ItemComponent == NULL) {
        QQmlContext* context = QQmlEngine::contextForObject(this);
        if(context == NULL) { return; }
        ItemComponent = new QQmlComponent(context->engine(), qmlSource(), this);
        if(ItemComponent->isError()) {
            qDebug() << "Point errors:" << ItemComponent->errorString();
        }
    }
}

/**
  This is a private method, that adds a new station the end of the station list

    Returns a valid item if the item was create okay, and NULL if it failed to be created
*/
QQuickItem* cwAbstractPointManager::addItem() {

    if(ItemComponent == NULL) {
        qDebug() << "PointComponent isn't ready, ... " << qmlSource() << "Didn't compile THIS IS A BUG" << LOCATION;
        return NULL;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQuickItem* item = qobject_cast<QQuickItem*>(ItemComponent->create(context));
    if(item == NULL) {
        qDebug() << "Problem creating new point item ... " << qmlSource() << "Didn't compile. THIS IS A BUG!" << LOCATION;
        return NULL;
    }

    Items.append(item);

    //Add the point to the transform updater
    if(TransformUpdater != NULL) {
        TransformUpdater->addPointItem(item);
    }

    return item;
}

/**
  Called when a point has been added
  */
void cwAbstractPointManager::pointAdded() {
    createComponent();

    if(ItemComponent == NULL) {
        qDebug() << "StationItemComponent bad" << LOCATION;
        return;
    }

    //Create a new station item
    QQuickItem* item = addItem();

    //Update the station
    if(item != NULL) {
        int lastIndex = Items.size() - 1;
        updateItemData(item, lastIndex);
        item->setParentItem(this);
    }
}

/**
  Handles when the point is removed
  */
void cwAbstractPointManager::pointRemoved(int index) {
    //Unselect the item that's going to be deleted
    if(index >= 0 && index < Items.size()) {
        if(selectedItemIndex() == index) {
            clearSelection();
        }

        Items[index]->deleteLater();
        Items.removeAt(index);

        updateAllItemData();
    }
}


/**
 * @brief cwAbstractPointManager::updateAll
 * @param numberOfStations - The number of point item that should be created.
 *
 * This fuction will try to reuse pre-existing.  This will all update station's data,
 * including the positions.
 */
void cwAbstractPointManager::resizeNumberOfItems(int numberOfPoints)
{
    //Make sure we have a note component so we can create it
    createComponent();

    if(Items.size() < numberOfPoints) {
        int notesToAdd = numberOfPoints - Items.size();
        //Add more stations to the NoteStations
        for(int i = 0; i < notesToAdd; i++) {
            addItem();
        }

    } else if(Items.size() > numberOfPoints) {
        //Remove stations from NoteStations
        int notesToRemove = Items.size() - numberOfPoints;

        //Remove stations to the NoteStations
        for(int i = 0; i < notesToRemove; i++) {
            QQuickItem* deleteStation = Items.last();
            Items.removeLast();
            deleteStation->deleteLater();
        }
    }

    updateAllItemData();
}


/**
  \brief Goes through all the stations and updates there data
  */
void cwAbstractPointManager::updateAllItemData()
{
    //Update all the station data
    for(int i = 0; i < Items.size(); i++) {
        updateItemData(Items.at(i), i);
    }
}

/**
Sets selectedStationIndex
*/
void cwAbstractPointManager::setSelectedItemIndex(int selectedIndex) {
    if(SelectedItemIndex != selectedIndex) {

       // scrapItem()->selectScrapStationView(this);

        if(selectedIndex >= Items.size()) {
            qDebug() << "Selected station index invalid" << selectedIndex << LOCATION;
            return;
        }

        //Deselect the old index
        QQuickItem* oldItem = selectedItem();
        if(oldItem != NULL) {
            oldItem->setProperty("selected", false);
        }

        //Select the new station item
        if(selectedIndex >= 0) {
            QQuickItem* newItem = Items.at(selectedIndex);
            if(!newItem->property("selected").toBool()) {
                newItem->setProperty("selected", true);
            }
        }

        SelectedItemIndex = selectedIndex;
//        updateShotLines();
        emit selectedItemIndexChanged();
    }
}

/**
  Gets the currently select station item

  If there's no select station item, this will return null
  */
QQuickItem* cwAbstractPointManager::selectedItem() const {
    if(SelectedItemIndex >= 0 && SelectedItemIndex < Items.size()) {
        return Items.at(SelectedItemIndex);
    }
    return NULL;
}

/**
     * @brief updatePosition
     * @param index - The index that needs to be updated
     */
void cwAbstractPointManager::updateItemPosition(int index) {
    if(index >= 0 && index < Items.size()) {
        updateItemPosition(Items.at(index), index);
    }
}

