/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAbstractPointManager.h"
#include "cwDebug.h"
#include "cwSelectionManager.h"

//Qt includes
#include <QQmlEngine>
#include <QQmlContext>

cwAbstractPointManager::cwAbstractPointManager(QQuickItem *parent) :
    QQuickItem(parent),
    m_selectionManager(nullptr),
    m_selectedItemIndex(-1)
{
    //Update the parent of the all the items so the scale and rotations
    //work correctly.
    connect(this, &cwAbstractPointManager::parentChanged,
            this, [this]()
            {
                for(auto item : std::as_const(m_items)) {
                    item->setParentItem(parentItem());
                }
            });
}

/**
  This creates the station component used generate the station symobols
  */
// void cwAbstractPointManager::createComponent() {
//     //Make sure we have a note component so we can create it
//     if(m_itemComponent == nullptr) {
//         QQmlContext* context = QQmlEngine::contextForObject(this);
//         if(context == nullptr) {
//             qDebug() << "Context is nullptr, did you forget to set the context? THIS IS A BUG" << LOCATION;
//             return;
//         }

//         m_itemComponent = new QQmlComponent(context->engine(), qmlSource(), this);
//         if(m_itemComponent->isError()) {
//             qDebug() << "Point errors:" << m_itemComponent->errorString();
//         }
//     }
// }

QQmlComponent* cwAbstractPointManager::component() const
{
    return m_component;
}

void cwAbstractPointManager::setComponent(QQmlComponent* comp)
{
    if(m_component == comp) {
        return;
    }

    m_component = comp;
    emit componentChanged();

    // Rebuild items because creation path may change
    resizeNumberOfItems(m_items.size());
    updateAllItemData();
}

QUrl cwAbstractPointManager::qmlSource() const
{
    // Default implementation returns the set URL (subclasses can override)
    return m_qmlSourceUrl;
}

void cwAbstractPointManager::setQmlSource(const QUrl& source)
{
    if(m_qmlSourceUrl == source) {
        return;
    }
    m_qmlSourceUrl = source;
    emit qmlSourceChanged();

    // If we are using URL path, clear any external component to avoid ambiguity
    if(!m_qmlSourceUrl.isEmpty()) {
        m_component = nullptr;
        emit componentChanged();
    }

    resizeNumberOfItems(m_items.size());
    updateAllItemData();
}


QQuickItem* cwAbstractPointManager::createItem(int index)
{
    ensureComponent();
    if(!ensureComponentReady()) {
        return nullptr;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    if(context == nullptr) {
        qDebug() << "Context is nullptr. THIS IS A BUG" << LOCATION;
        return nullptr;
    }

    QObject* obj = m_component->beginCreate(context);
    QQuickItem* item = qobject_cast<QQuickItem*>(obj);
    if(item == nullptr) {
        if(obj != nullptr) {
            obj->deleteLater();
        }
        qDebug() << "Delegate isn't a QQuickItem" << LOCATION;
        return nullptr;
    }

    // NEW: seed required properties before completeCreate()
    const QVariantMap props = initialItemProperties(item, index);
    if(!props.isEmpty()) {
        m_component->setInitialProperties(item, props);
    }

    item->setParentItem(this);
    item->setParent(this);

    m_component->completeCreate();

    if(m_component->isError()) {
        qDebug() << "Item component errors:" << m_component->errorString();
        return nullptr;
    }

    return item;
}

void cwAbstractPointManager::refreshItemAt(int index)
{
    if(index < 0 || index >= m_items.size()) {
        return;
    }
    privateUpdateItemData(m_items.at(index), index);
}

// /**
//   This is a private method, that adds a new station the end of the station list

//     Returns a valid item if the item was create okay, and nullptr if it failed to be created
// */
// QQuickItem *cwAbstractPointManager::createItem() {

//     if(m_itemComponent == nullptr) {
//         qDebug() << "ItemComponent hasn't been created, call createComponent(). THIS IS A BUG" << LOCATION;
//         return nullptr;
//     }

//     QQmlContext* context = QQmlEngine::contextForObject(this);
//     QQuickItem* item = qobject_cast<QQuickItem*>(m_itemComponent->create(context));
//     if(item == nullptr) {
//         qDebug() << "Problem creating new point item ... " << qmlSource() << "Didn't compile. THIS IS A BUG!" << LOCATION;
//         qDebug() << "Compiling errors:" << m_itemComponent->errorString();
//         return nullptr;
//     }

//     //Reparent the item so the reverse transform works correctly
//     //This will keep the item's size and rotation consistent
//     item->setParentItem(parentItem());

//     // if(m_pointParentItem) {
//     //     item->setParentItem(m_pointParentItem);
//     // }

//     //Add the point to the transform updater
//     // if(TransformUpdater != nullptr) {
//     //     TransformUpdater->addChildItem(item);
//     // } else {
//     //     qDebug() << "No transformUpdater, point's won't be positioned correctly, this is a bug" << LOCATION;
//     // }

//     return item;
// }

/**
  Called when a point has been added
  */
void cwAbstractPointManager::pointAdded() {
    int lastIndex = m_items.size();
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
    ensureComponent();
    if(!ensureComponentReady()) {
        return;
    }

    for(int i = begin; i <= end; i++) {
        QQuickItem* item = createItem(i); // pass index
        if(item == nullptr) {
            qDebug() << "Can't insert. Item is nullptr. THIS IS A BUG" << LOCATION;
            break;
        }

        m_items.insert(i, item);
        onItemCreated(item, i);
        privateUpdateItemData(item, i);
    }

    for(int i = end + 1; i < m_items.size(); i++) {
        privateUpdateItemData(m_items.at(i), i);
    }


    // createComponent();

    // for(int i = begin; i <= end; i++) {
    //     auto item = createItem();

    //     if(item != nullptr) {
    //         m_items.insert(i, item);
    //         privateUpdateItemData(item, i);
    //     } else {
    //         qDebug() << "Can't insert. Item is nullptr. THIS IS A BUG" << LOCATION;
    //         break;
    //     }
    // }

    // //Update indices for all items after end
    // for(int i = end + 1; i < m_items.size(); i++) {
    //     privateUpdateItemData(m_items.at(i), i);
    // }
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
    if(m_items.isEmpty()) { return; }
    if(begin > end) { return; }
    if(begin < 0) { return; }
    if(end >= m_items.size()) { return; }

    for(int index = end; index >= begin; index--) {
        if(selectedItemIndex() == index) {
            clearSelection();
        }
        onItemAboutToBeDestroyed(m_items[index], index);
        m_items[index]->deleteLater();
        m_items.removeAt(index);
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

void cwAbstractPointManager::ensureComponent()
{
    if(m_component) {
        return;
    }

    if(!isComponentComplete()) {
        return;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    if(context == nullptr) {
        qDebug() << "Context is nullptr. THIS IS A BUG" << LOCATION;
        return;
    }

    const QUrl sourceUrl = !m_qmlSourceUrl.isEmpty() ? m_qmlSourceUrl : qmlSource();

    if(!sourceUrl.isEmpty()) {
        m_component = new QQmlComponent(context->engine(), sourceUrl, this);
        if(m_component->isError()) {
            qDebug() << "Point component errors:" << m_component->errorString();
        }
    } else {
        // No provided component, no source URL: subclasses must override qmlSource() or set component
        qDebug() << "No component or qmlSource provided for cwAbstractPointManager" << LOCATION;
    }
}

bool cwAbstractPointManager::ensureComponentReady() const
{
    if(!m_component) {
        return false;
    }
    if(m_component->isError()) {
        qDebug() << "Item component errors:" << m_component->errorString();
        return false;
    }
    return m_component->status() == QQmlComponent::Ready;
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
    ensureComponent();

    if(m_items.size() < numberOfPoints) {
        const int toAdd = numberOfPoints - m_items.size();
        for(int k = 0; k < toAdd; k++) {
            const int index = m_items.size(); // new item will be appended here
            QQuickItem* item = createItem(index);
            if(item != nullptr) {
                m_items.append(item);
                onItemCreated(item, index);
            }
        }
    } else if(m_items.size() > numberOfPoints) {
        const int toRemove = m_items.size() - numberOfPoints;
        for(int k = 0; k < toRemove; k++) {
            QQuickItem* deleteItem = m_items.last();
            const int idx = m_items.size() - 1;
            onItemAboutToBeDestroyed(deleteItem, idx);
            m_items.removeLast();
            deleteItem->deleteLater();
        }
    }

    updateAllItemData();
}

    // //Make sure we have a note component so we can create it
    // createComponent();

    // if(m_items.size() < numberOfPoints) {
    //     int notesToAdd = numberOfPoints - m_items.size();
    //     //Add more stations to the NoteStations
    //     for(int i = 0; i < notesToAdd; i++) {
    //         auto item = createItem();
    //         m_items.append(item);
    //     }

    // } else if(m_items.size() > numberOfPoints) {
    //     //Remove stations from NoteStations
    //     int notesToRemove = m_items.size() - numberOfPoints;

    //     //Remove stations to the NoteStations
    //     for(int i = 0; i < notesToRemove; i++) {
    //         auto deleteStation = m_items.last();
    //         m_items.removeLast();
    //         deleteStation->deleteLater();
    //     }
    // }

    // updateAllItemData();
// }


/**
  \brief Goes through all the stations and updates there data
  */
void cwAbstractPointManager::updateAllItemData()
{
    //Update all the station data
    for(int i = 0; i < m_items.size(); i++) {
        privateUpdateItemData(m_items.at(i), i);
    }
}

/**
 * @brief cwAbstractPointManager::setSelectedItemIndex
 * @param selectedIndex - The item that should be selected. Set to -1 to deselect the current item
 */
void cwAbstractPointManager::setSelectedItemIndex(int selectedIndex) {
    if(m_selectedItemIndex != selectedIndex) {
        if(selectedIndex >= m_items.size()) {
            qDebug() << "Selected station index invalid" << selectedIndex << LOCATION;
            return;
        }

        m_selectedItemIndex = selectedIndex;

        //Select the new station item
        if(selectedIndex >= 0) {
            QQuickItem* newItem = m_items.at(selectedIndex);
            if(selectionManager() != nullptr) {
                selectionManager()->setSelectedItem(newItem);
            } else {
                qDebug() << "Selection manager isn't set!!!" << LOCATION;
            }
        } else {
            selectionManager()->setSelectedItem(nullptr); //deselect the current item
        }

        emit selectedItemIndexChanged();
    }
}

/**
  Gets the currently select station item

  If there's no select station item, this will return null
  */
QQuickItem* cwAbstractPointManager::selectedItem() const {
    if(m_selectedItemIndex >= 0 && m_selectedItemIndex < m_items.size()) {
        QQuickItem* item = m_items.at(m_selectedItemIndex);
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
    return m_items;
}

/**
     * @brief updatePosition
     * @param index - The index that needs to be updated
     */
void cwAbstractPointManager::privateUpdateItemPosition(int index) {
    if(index >= 0 && index < m_items.size()) {
        updateItemPosition(m_items.at(index), index);
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

    updateItemData(item, index);
    privateUpdateItemPosition(index);
}

/**
Sets selectionManager
*/
void cwAbstractPointManager::setSelectionManager(cwSelectionManager* selectionManager) {
    if(m_selectionManager != selectionManager) {

        if(m_selectionManager != nullptr) {
            disconnect(m_selectionManager, &cwSelectionManager::selectedItemChanged, this, &cwAbstractPointManager::updateSelection);
        }

        m_selectionManager = selectionManager;

        if(m_selectionManager != nullptr) {
            connect(m_selectionManager, &cwSelectionManager::selectedItemChanged, this, &cwAbstractPointManager::updateSelection);
        }

        emit selectionManagerChanged();
    }
}
