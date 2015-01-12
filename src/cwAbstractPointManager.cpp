/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAbstractPointManager.h"
#include "cwTransformUpdater.h"
#include "cwDebug.h"
#include "cwSelectionManager.h"

//Qt includes
#include <QQmlEngine>
#include <QQmlContext>

cwAbstractPointManager::cwAbstractPointManager(QQuickItem *parent) :
    QQuickItem(parent),
    TransformUpdater(nullptr),
    ItemComponent(nullptr),
    SelectionManager(nullptr),
    SelectedItemIndex(-1)
{

}

/**
  \brief Sets the transform updater

  This will transform all the station in this station view
  */
void cwAbstractPointManager::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {
        if(TransformUpdater != nullptr) {
            //Remove all previous stations
            foreach(QQuickItem* item, Items) {
                TransformUpdater->removePointItem(item);
            }
        }

        TransformUpdater = updater;

        if(TransformUpdater != nullptr) {
            //Add station to the new transformUpdater

            foreach(QQuickItem* item, Items) {
                TransformUpdater->addPointItem(item);
            }
        }

        emit transformUpdaterChanged();
    }
}

/**
  This creates the station component used generate the station symobols
  */
void cwAbstractPointManager::createComponent() {
    //Make sure we have a note component so we can create it
    if(ItemComponent == nullptr) {
        QQmlContext* context = QQmlEngine::contextForObject(this);
        if(context == nullptr) {
            qDebug() << "Context is nullptr, did you forget to set the context? THIS IS A BUG" << LOCATION;
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

    Returns a valid item if the item was create okay, and nullptr if it failed to be created
*/
QQuickItem* cwAbstractPointManager::createItem() {

    if(ItemComponent == nullptr) {
        qDebug() << "ItemComponent hasn't been created, call createComponent(). THIS IS A BUG" << LOCATION;
        return nullptr;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQuickItem* item = qobject_cast<QQuickItem*>(ItemComponent->create(context));
    if(item == nullptr) {
        qDebug() << "Problem creating new point item ... " << qmlSource() << "Didn't compile. THIS IS A BUG!" << LOCATION;
        qDebug() << "Compiling errors:" << ItemComponent->errorString();
        return nullptr;
    }

    //Add the point to the transform updater
    if(TransformUpdater != nullptr) {
        TransformUpdater->addPointItem(item);
    } else {
        qDebug() << "No transformUpdater, point's won't be positioned correctly, this is a bug" << LOCATION;
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

        if(item != nullptr) {
            Items.insert(i, item);
            privateUpdateItemData(item, i);
        } else {
            qDebug() << "Can't insert. Item is nullptr. THIS IS A BUG" << LOCATION;
            break;
        }
    }

    //Update indices for all items after end
    for(int i = end + 1; i < Items.size(); i++) {
        privateUpdateItemData(Items.at(i), i);
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
 * @brief cwAbstractPointManager::updateItemsPositions
 * @param begin - The first item that needs their position updated
 * @param end - The last item that needs their position updated
 */
void cwAbstractPointManager::updateItemsPositions(int begin, int end)
{
    for(int i = begin; i <= end; i++) {
        privateUpdateItemPosition(i);
    }
}

/**
 * @brief cwAbstractPointManager::updateSelection
 *
 * Update the selection, when the selection manager has changed the selection
 */
void cwAbstractPointManager::updateSelection()
{
    //SelectedItem test if the item is
    if(selectedItem() == nullptr) {
        setSelectedItemIndex(-1);
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
        if(selectedIndex >= Items.size()) {
            qDebug() << "Selected station index invalid" << selectedIndex << LOCATION;
            return;
        }

        SelectedItemIndex = selectedIndex;

        //Select the new station item
        if(selectedIndex >= 0) {
            QQuickItem* newItem = Items.at(selectedIndex);
            if(selectionManager() != nullptr) {
                selectionManager()->setSelectedItem(newItem);
            }
        }

        emit selectedItemIndexChanged();
    }
}

/**
  Gets the currently select station item

  If there's no select station item, this will return null
  */
QQuickItem* cwAbstractPointManager::selectedItem() const {
    if(SelectedItemIndex >= 0 && SelectedItemIndex < Items.size()) {
        QQuickItem* item = Items.at(SelectedItemIndex);
        if(selectionManager() != nullptr) {
            if(selectionManager()->isSelected(item)) {
                return item;
            }
        }
    }
    return nullptr;
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
void cwAbstractPointManager::privateUpdateItemPosition(int index) {
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
    if(item == nullptr) { return; }

    //Update assumed stuff
    item->setProperty("pointIndex", index);
    item->setProperty("parentView", QVariant::fromValue(this));

    if(item->parentItem() != this) {
        item->setParentItem(this);
        item->setParent(this);
    }

    updateItemData(item, index);
    privateUpdateItemPosition(index);
}

/**
Sets selectionManager
*/
void cwAbstractPointManager::setSelectionManager(cwSelectionManager* selectionManager) {
    if(SelectionManager != selectionManager) {

        if(SelectionManager != nullptr) {
            disconnect(SelectionManager, &cwSelectionManager::selectedItemChanged, this, &cwAbstractPointManager::updateSelection);
        }

        SelectionManager = selectionManager;

        if(SelectionManager != nullptr) {
            connect(SelectionManager, &cwSelectionManager::selectedItemChanged, this, &cwAbstractPointManager::updateSelection);
        }

        emit selectionManagerChanged();
    }
}

