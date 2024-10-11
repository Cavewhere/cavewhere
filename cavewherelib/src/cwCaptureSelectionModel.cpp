/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwCaptureSelectionModel.h"
#include "cwCaptureItem.h"

cwCaptureSelectionModel::cwCaptureSelectionModel(QObject *parent) :
    QObject(parent)
{
}

/**
 * @brief cwCaptureSelectionModel::select
 * @param item
 *
 * This is a convenience function for clear() and addToSelection(item). This will remove all
 * other selections and select item.
 */
void cwCaptureSelectionModel::select(cwCaptureItem *item)
{
    clear();
    addToSelection(item);
}

/**
 * @brief cwCaptureSelectionModel::addToSelection
 * @param item
 *
 * This will add item to the selection list. This will not clear the list. This function
 * is useful for selecting multiple items.
 *
 * If the item aleardy exists in the list, this function will do nothing. If the item is nullptr
 * this function will also do nothing.
 */
void cwCaptureSelectionModel::addToSelection(cwCaptureItem *item)
{
    if(item == nullptr) { return; }
    if(SelectedItems.contains(item)) { return; }

    connect(item, &cwCaptureItem::destroyed, this, &cwCaptureSelectionModel::removeDeleted);
    SelectedItems.insert(item);
    emit selectedItemsChanged();
}

/**
 * @brief cwCaptureSelectionModel::removeFromSelection
 * @param item
 *
 * This will remove the selection from the list. If item doesn't exist in the list
 * this function will do nothing.
 */
void cwCaptureSelectionModel::removeFromSelection(cwCaptureItem *item)
{
    if(removeHelper(item)) {
        emit selectedItemsChanged();
    }

}

/**
 * @brief cwCaptureSelectionModel::clear
 *
 * Clears the selection.
 *
 * If the selection is already empty, this does nothing.
 */
void cwCaptureSelectionModel::clear()
{
    if(SelectedItems.isEmpty()) { return; }

    foreach(cwCaptureItem* item, SelectedItems) {
        removeHelper(item);
    }

    emit selectedItemsChanged();
}

/**
 * @brief cwCaptureSelectionModel::removeHelper
 * @param item
 * @return true if successfully remove and false if unsuccessful
 *
 * This is a helper function for removing item.
 */
bool cwCaptureSelectionModel::removeHelper(cwCaptureItem *item)
{
    if(item == nullptr) { return false; }
    if(SelectedItems.contains(item)) {
        disconnect(item, &cwCaptureItem::destroyed, this, &cwCaptureSelectionModel::removeDeleted);
        SelectedItems.insert(item);
        return true;
    }
    return false;
}

/**
 * @brief cwCaptureSelectionModel::removeDeleted
 *
 * This function is called when an item is delete and the list needs to
 * be updated.
 */
void cwCaptureSelectionModel::removeDeleted(QObject* item)
{
    cwCaptureItem* captureItem = static_cast<cwCaptureItem*>(item);
    Q_ASSERT(SelectedItems.contains(captureItem));
    SelectedItems.remove(captureItem);
}
