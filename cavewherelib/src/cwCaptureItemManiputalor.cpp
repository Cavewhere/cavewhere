/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include "cwCaptureItemManiputalor.h"
#include "cwCaptureManager.h"
#include "cwCaptureItem.h"
#include "cwGlobalDirectory.h"
#include "cwDebug.h"


//Our includes
#include <QQmlEngine>
#include <QQmlContext>
#include <QVariant>

cwCaptureItemManiputalor::cwCaptureItemManiputalor(QQuickItem *parent) :
    QQuickItem(parent),
    InteractionComponent(nullptr)
{
    connect(this, &QQuickItem::xChanged,
            this, &cwCaptureItemManiputalor::updateTransform);
    connect(this, &QQuickItem::yChanged,
            this, &cwCaptureItemManiputalor::updateTransform);
    connect(this, &QQuickItem::widthChanged,
            this, &cwCaptureItemManiputalor::updateTransform);
    connect(this, &QQuickItem::heightChanged,
            this, &cwCaptureItemManiputalor::updateTransform);
}

/**
* @brief cwCaptureLayerManiputalor::setManager
* @param manager
*/
void cwCaptureItemManiputalor::setManager(cwCaptureManager* manager) {
    if(Manager != manager) {
        if(!Manager.isNull()) {
            disconnect(manager, &cwCaptureManager::modelReset, this, &cwCaptureItemManiputalor::fullUpdate);
            disconnect(manager, &cwCaptureManager::rowsInserted, this, &cwCaptureItemManiputalor::insertItems);
            disconnect(manager, &cwCaptureManager::rowsAboutToBeRemoved, this, &cwCaptureItemManiputalor::removeItems);
            disconnect(manager->scene(), SIGNAL(sceneRectChanged(QRectF)), this, SLOT(updateTransform()));
            disconnect(manager, &cwCaptureManager::aboutToDestoryManager, this, &cwCaptureItemManiputalor::managerDestroyed);
        }

        Manager = manager;

        if(!Manager.isNull()) {
            connect(manager, &cwCaptureManager::modelReset, this, &cwCaptureItemManiputalor::fullUpdate);
            connect(manager, &cwCaptureManager::rowsInserted, this, &cwCaptureItemManiputalor::insertItems);
            connect(manager, &cwCaptureManager::rowsAboutToBeRemoved, this, &cwCaptureItemManiputalor::removeItems);
            connect(manager->scene(), SIGNAL(sceneRectChanged(QRectF)), this, SLOT(updateTransform()));
            connect(manager, &cwCaptureManager::aboutToDestoryManager, this, &cwCaptureItemManiputalor::managerDestroyed);
        }

        emit managerChanged();

        fullUpdate();
        updateTransform();
    }
}

/**
 * @brief cwCaptureItemManiputalor::fullUpdate
 *
 * This will clear the current interaction items, and
 */
void cwCaptureItemManiputalor::fullUpdate()
{
    if(Manager.isNull()) { return; }

    if(InteractionComponent == nullptr) {
        QQmlContext* context = QQmlEngine::contextForObject(this);
        InteractionComponent = new QQmlComponent(context->engine(),
                                                 QStringLiteral("qrc:/cavewherelib/cavewherelib/CaptureItemInteraction.qml"),
                                                 this);
        cwDebug::printErrors(InteractionComponent);
    }

    //Inserts all the items
    insertItems(QModelIndex(), 0, Manager->rowCount() - 1);
}

/**
 * @brief cwCaptureItemManiputalor::insertItems
 * @param parent - unused, always send a QModelIndex()
 * @param start - The starting index of the insert
 * @param end - The ending index of the insert
 *
 * This will insert new item's between start and end. If end because before start, this
 * function will do nothing.
 */
void cwCaptureItemManiputalor::insertItems(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);

    QVariantMap requiredProperties {{"quickSceneView", QVariant::fromValue(m_view.data())},
                                   {"captureItem", QVariant::fromValue(nullptr)},
                                   {"captureScale", PaperToScreenScale},
                                   {"captureOffset", SceneOffset}};

    for(int i = start; i <= end; i++) {
        cwCaptureItem* item = captureItem(i);

        Q_ASSERT(item != nullptr);

        if(!CaptureToQuickItem.contains(item)) {
            requiredProperties["captureItem"] = QVariant::fromValue(item);
            QQuickItem* quickItem = createInteractionItem(requiredProperties);

            CaptureToQuickItem.insert(item, quickItem);
            QuickItemToCapture.insert(quickItem, item);
        }
    }
}

/**
 * @brief cwCaptureItemManiputalor::removeItems
 * @param parent
 * @param start
 * @param end
 */
void cwCaptureItemManiputalor::removeItems(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);

    for(int i = start; i <= end; i++) {
        cwCaptureItem* item = captureItem(i);

        if(CaptureToQuickItem.contains(item)) {
            QQuickItem* quickItem = CaptureToQuickItem.value(item);
            CaptureToQuickItem.remove(item);
            QuickItemToCapture.remove(quickItem);
            quickItem->deleteLater();
        }
    }
}

/**
 * @brief cwCaptureItemManiputalor::updateScale
 */
void cwCaptureItemManiputalor::updateTransform()
{
   PaperToScreenScale = width() / Manager->scene()->sceneRect().width();
   SceneOffset = Manager->scene()->sceneRect().topLeft();
   foreach(QQuickItem* item, QuickItemToCapture.keys()) {
       updateItemTransform(item);
   }
}

/**
 * @brief cwCaptureItemManiputalor::managerDestroyed
 * @param object
 */
void cwCaptureItemManiputalor::managerDestroyed()
{
    foreach(QQuickItem* item, QuickItemToCapture.keys()) {
        delete item;
    }
    QuickItemToCapture.clear();
    CaptureToQuickItem.clear();
}

/**
 * @brief cwCaptureItemManiputalor::createInteractionItem
 * @return This creates a instance of InteractionComponent
 */
QQuickItem *cwCaptureItemManiputalor::createInteractionItem(const QVariantMap& requiredProperties)
{
    Q_ASSERT(InteractionComponent != nullptr);
    qDebug() << "Insert item:" << requiredProperties;
    QObject* object = InteractionComponent->createWithInitialProperties(requiredProperties);
    QQuickItem* item = dynamic_cast<QQuickItem*>(object);
    Q_ASSERT(item != nullptr);

    item->setParentItem(this);

    return item;
}

/**
 * @brief cwCaptureItemManiputalor::clear
 *
 * This will go through all the interaction items and delete's them.
 */
void cwCaptureItemManiputalor::clear()
{
    foreach(QQuickItem* items, QuickItemToCapture.keys()) {
        items->deleteLater();
    }
    QuickItemToCapture.clear();
    CaptureToQuickItem.clear();
}

/**
 * @brief cwCaptureItemManiputalor::updateScale
 * @param item
 *
 * This updates the scale for the item. Using the PaperToScreenScale. The item's
 * native's coordinate system is in paper units. So it must be scaled so it shows
 * up correctly over top of the cwQuickSceneView.
 */
void cwCaptureItemManiputalor::updateItemTransform(QQuickItem *item)
{
    qDebug() << "Update Transform:" << PaperToScreenScale << SceneOffset;
   item->setProperty("captureScale", PaperToScreenScale);
   item->setProperty("captureOffset", SceneOffset);
}

/**
 * @brief cwCaptureItemManiputalor::captureItem
 * @param i
 * @return Returns the catpure item at i from the model
 *
 * This uses the i as the index to the model and get's the catpureItem from it.
 */
cwCaptureItem *cwCaptureItemManiputalor::captureItem(int i) const
{
    QModelIndex index = Manager->index(i);
    QVariant itemObjectVariant = Manager->data(index, cwCaptureManager::LayerObjectRole);
    cwCaptureItem* item = itemObjectVariant.value<cwCaptureItem*>();
    return item;
}

/**
* @brief cwCaptureLayerManiputalor::manager
* @return
*/
cwCaptureManager* cwCaptureItemManiputalor::manager() const {
    return Manager;
}

cwQuickSceneView *cwCaptureItemManiputalor::view() const
{
    return m_view;
}

void cwCaptureItemManiputalor::setView(cwQuickSceneView *newView)
{
    if (m_view == newView) {
        return;
    }
    m_view = newView;
    emit viewChanged();
}
