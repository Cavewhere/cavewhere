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
        if(context == NULL) {
            qDebug() << "Context is NULL, did you forget to set the context? THIS IS A BUG" << LOCATION;
            return;
        }

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
QQuickItem* cwAbstractPointManager::createItem() {

    if(ItemComponent == NULL) {
        qDebug() << "ItemComponent hasn't been created, call createComponent(). THIS IS A BUG" << LOCATION;
        return NULL;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQuickItem* item = qobject_cast<QQuickItem*>(ItemComponent->create(context));
    if(item == NULL) {
        qDebug() << "Problem creating new point item ... " << qmlSource() << "Didn't compile. THIS IS A BUG!" << LOCATION;
        return NULL;
    }

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
    int lastIndex = Items.size();
    pointsInserted(lastIndex, lastIndex);
}

/**
  Handles when the point is removed
  */
void cwAbstractPointManager::pointRemoved(int index) {
    pointsRemoved(index, index);
}

/**
 * @brief cwAbstractPointManager::pointsInserted
 * @param begin - The index where the points were added
 * @param end - The end index where the points were added
 *
 * Adds new points to the point manager between begin and end
 */
void cwAbstractPointManager::pointsInserted(int begin, int end)
{
    createComponent();

    for(int i = begin; i <= end; i++) {
        QQuickItem* item = createItem();

        if(item != NULL) {
            Items.insert(i, item);
            privateUpdateItemData(item, i);
            item->setParentItem(this);
        } else {
            qDebug() << "Can't insert. Item is NULL. THIS IS A BUG" << LOCATION;
            break;
        }
    }
}

/**
 * @brief cwAbstractPointManager::pointsRemoved
 * @param begin - The first index that will be removed
 * @param end - The last index that will be removed
 *
 * Removes all the points between begin and end
 */
void cwAbstractPointManager::pointsRemoved(int begin, int end)
{
    if(Items.isEmpty()) { return; }
    if(begin > end) { return; }
    if(begin < 0) { return; }
    if(end >= Items.size()) { return; }

    for(int index = end; index >= begin; index--) {
        //Unselect the item that's going to be deleted

        if(selectedItemIndex() == index) {
            clearSelection();
        }

        Items[index]->deleteLater();
        Items.removeAt(index);
    }

    updateAllItemData();
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
            QQuickItem* item = createItem();
            Items.append(item);
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
        privateUpdateItemData(Items.at(i), i);
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
 * @brief cwAbstractPointManager::items
 * @return - Returns a list of all the items in the point manager
 */
QList<QQuickItem *> cwAbstractPointManager::items() const
{
    return Items;
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

/**
 * @brief cwAbstractPointManager::privateUpdateItemData
 * @param item - The item that will be update
 * @param index - The index of the item
 *
 * This will update the item's data with the subclass, and also update the
 * position of the item
 */
void cwAbstractPointManager::privateUpdateItemData(QQuickItem* item, int index)
{
    updateItemData(item, index);
    updateItemPosition(index);
}
