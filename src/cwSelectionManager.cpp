/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSelectionManager.h"

//Qt includes
#include <QQuickItem>

cwSelectionManager::cwSelectionManager(QObject *parent) :
    QObject(parent),
    SelectedItem(NULL)
{

}

/**
Sets selectedItem
*/
void cwSelectionManager::setSelectedItem(QQuickItem* selectedItem) {
    if(SelectedItem != selectedItem) {

        if(SelectedItem != NULL) {
            disconnect(SelectedItem, SIGNAL(destroyed()), this, SLOT(selectedItemDestroyed()));
            SelectedItem->setProperty("selected", false);
        }

        SelectedItem = selectedItem;

        if(SelectedItem != NULL) {

            connect(SelectedItem, SIGNAL(destroyed()), this, SLOT(selectedItemDestroyed()));

            if(!SelectedItem->property("selected").toBool()) {
                SelectedItem->setProperty("selected", true);
            }
        }

        emit selectedItemChanged();
    }
}

/**
 * @brief cwSelectionManager::selectedItemDestroyed
 *
 * Called when the selected item has been destroyed
 */
void cwSelectionManager::selectedItemDestroyed()
{
    SelectedItem = NULL;
    emit selectedItemChanged();
}

