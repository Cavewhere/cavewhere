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
    SelectedItem(nullptr)
{

}

/**
Sets selectedItem
*/
void cwSelectionManager::setSelectedItem(QQuickItem* selectedItem) {
    if(SelectedItem != selectedItem) {

        if(SelectedItem != nullptr) {
            disconnect(SelectedItem, SIGNAL(destroyed()), this, SLOT(selectedItemDestroyed()));
            SelectedItem->setProperty("selected", false);
        }

        SelectedItem = selectedItem;
        m_cycleQueue.removeOne(selectedItem);

        if(SelectedItem != nullptr) {

            connect(SelectedItem, SIGNAL(destroyed()), this, SLOT(selectedItemDestroyed()));

            if(!SelectedItem->property("selected").toBool()) {
                SelectedItem->setProperty("selected", true);
            }
        }

        emit selectedItemChanged();
    }
}

void cwSelectionManager::cycleSelectItem(QQuickItem *item, ulong timestamp)
{
    if(m_cycleTimestamp != timestamp) {
        m_cycleTimestamp = timestamp;
        m_cycleSelects.clear();
    }

    m_cycleSelects.append(item);

    if(!m_cylceUpdateQueued) {
        m_cylceUpdateQueued = true;

        //This has to be queued because this function can be called multiple times
        //by a TapHandaler from multiple objects. This should only be queued once
        //per TapEvent based on the timestamp
        QMetaObject::invokeMethod(this, [this]() {

            //Remove items from the queue that don't exist in m_cycleSelects
            m_cycleQueue.erase(
                std::remove_if(
                    m_cycleQueue.begin(),
                    m_cycleQueue.end(),
                    [this](QQuickItem* item) {
                        // Check if the item exists in m_cycleSelects
                        return !m_cycleSelects.contains(item);
                    }
                    ),
                m_cycleQueue.end()
                );

            //Add new elements
            for(QQuickItem* item : m_cycleSelects) {
                if(!m_cycleQueue.contains(item)) {
                    m_cycleQueue.append(item);
                }
            }

            //Remove from the queue
            auto itemToSelect = m_cycleQueue.takeFirst();

            m_cylceUpdateQueued = false;
            setSelectedItem(itemToSelect);
        }, Qt::QueuedConnection);
    }
}

/**
 * @brief cwSelectionManager::selectedItemDestroyed
 *
 * Called when the selected item has been destroyed
 */
void cwSelectionManager::selectedItemDestroyed()
{
    SelectedItem = nullptr;
    emit selectedItemChanged();
}

