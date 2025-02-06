/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLeadView.h"
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwScrapLeadView.h"
#include "cwTransformUpdater.h"
#include "cwSelectionManager.h"
#include "cwDebug.h"

//Qt includes
#include <QQmlEngine>
#include <QQmlContext>

cwLeadView::cwLeadView(QQuickItem *parent) :
    QQuickItem(parent),
    TransformUpdater(new cwTransformUpdater(this)),
    SelectionMananger(new cwSelectionManager(this))
{
    connect(TransformUpdater, &cwTransformUpdater::cameraChanged, this, &cwLeadView::cameraChanged);
    connect(this, &cwLeadView::visibleChanged, this, [this](){
        TransformUpdater->setEnabled(isVisible());
    });

}

cwLeadView::~cwLeadView()
{

}

/**
* @brief cwLeadView::regionModel
* @return The regionModel that this leadView looks for leads in
*/
cwRegionTreeModel* cwLeadView::regionModel() const {
    return RegionModel;
}

/**
* @brief cwLeadView::setRegionModel
* @param regionModel
*
* Sets the region model that is used to listen to scraps being added or removed
*/
void cwLeadView::setRegionModel(cwRegionTreeModel* regionModel) {
    if(RegionModel != regionModel) {
        if(!RegionModel.isNull()) {
            disconnect(RegionModel.data(), &cwRegionTreeModel::rowsInserted, this, &cwLeadView::scrapsAdded);
            disconnect(RegionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved, this, &cwLeadView::scrapsRemoved);
        }

        RegionModel = regionModel;

        if(!RegionModel.isNull()) {
            connect(RegionModel.data(), &cwRegionTreeModel::rowsInserted, this, &cwLeadView::scrapsAdded);
            connect(RegionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved, this, &cwLeadView::scrapsRemoved);

            QList<cwScrap*> scraps = RegionModel->all<cwScrap*>(QModelIndex(), &cwRegionTreeModel::scrap);
            for(auto scrap : scraps) {
                addScrap(scrap);
            }
        }

        emit regionModelChanged();
    }
}

/**
 * @brief cwLeadView::addScrap
 * @param scrap - Adds the scrap to the LeadView
 */
void cwLeadView::addScrap(cwScrap *scrap)
{
    Q_ASSERT(scrap != nullptr);
    Q_ASSERT(!m_leadItems.contains(scrap));

    auto itemAt = [scrap](const auto& items, int i) {
        Q_ASSERT(dynamic_cast<QQuickItem*>(items[i]));
        return static_cast<QQuickItem*>(items[i]);
    };

    auto updatePosition = [scrap](QQuickItem* item, int i) {
        auto position = scrap->leadData(cwScrap::LeadPosition, i).value<QVector3D>();
        item->setProperty("position3D", position);
    };

    auto updatePositions = [scrap, this, updatePosition, itemAt](int begin, int end) {
        const auto& items = m_leadItems.value(scrap);
        for(int i = begin; i <= end; i++) {
            auto item = itemAt(items, i);
            updatePosition(item, i);
        }
    };

    auto resetScrap = [this, scrap, updatePosition, itemAt]() {
        auto& items = m_leadItems[scrap];
        if(items.size() < scrap->numberOfLeads()) {
            //Need to add new lead items
        } else if(items.size() > scrap->numberOfLeads()) {
            //Need to remove lead items
        }
        Q_ASSERT(items.size() == scrap->numberOfLeads());

        for(int i = 0; i < scrap->numberOfLeads(); i++) {
            //Update data for the item
            auto item = itemAt(items, i);

            auto position = scrap->leadData(cwScrap::LeadPosition, i).value<QVector3D>();
            item->setProperty("pointIndex", i);
            updatePosition(item, i);
        }
    };

    auto updateIndexesToEnd = [scrap, itemAt, this](int begin) {
        auto& items = m_leadItems[scrap];
        for(int i = begin; i < scrap->numberOfLeads(); i++) {
            auto item = itemAt(items, i);
            item->setProperty("pointIndex", i);
        }
    };

    auto beginInsert = [this, scrap, updateIndexesToEnd, updatePosition](int begin, int end) {
        if(begin <= end) {
            auto& items = m_leadItems[scrap];

            for(int i = begin; i <= end; i++) {
                //Create the item
                auto item = createItem();
                items.insert(i, item);
            }

            updateIndexesToEnd(begin);
        }
    };

    auto insert = [this, scrap, updatePosition, itemAt](int begin, int end) {
        auto& items = m_leadItems[scrap];
        for(int i = begin; i <= end; i++) {
            auto item = itemAt(items, i);
            updatePosition(item, i);
            item->setProperty("scrap", QVariant::fromValue(scrap));
        }
    };

    connect(scrap, &cwScrap::leadsBeginInserted, this, beginInsert);
    connect(scrap, &cwScrap::leadsInserted, this, insert);

    connect(scrap, &cwScrap::leadsRemoved, this, [this, scrap, updateIndexesToEnd](int begin, int end) {
        auto& items = m_leadItems[scrap];
        if(items.isEmpty()) { return; }
        if(begin > end) { return; }
        if(begin < 0) { return; }
        if(end >= items.size()) { return; }


        for(int index = end; index >= begin; index--) {
            //Unselect the item that's going to be deleted
            SelectionMananger->clear();
            TransformUpdater->removePointItem(items[index]);
            items[index]->deleteLater();
            items.removeAt(index);
        }


        updateIndexesToEnd(begin);
    });

    connect(scrap, &cwScrap::leadsDataChanged, this, [this, scrap, updatePositions](int begin, int end, const QList<int>& roles) {
        if(roles.contains(cwScrap::LeadPosition)) {
            updatePositions(begin, end);
        }
    });

    connect(scrap, &cwScrap::leadsReset, this, resetScrap);

    //Setup the leads
    m_leadItems.insert(scrap, {});

    auto lastIndex = scrap->numberOfLeads() - 1;
    beginInsert(0, lastIndex);
    insert(0, lastIndex);


    // QQmlContext* context = QQmlEngine::contextForObject(this);
    // cwScrapLeadView* leadView = new cwScrapLeadView(this);
    // QQmlEngine::setContextForObject(leadView, context);
    // leadView->setWidth(width());
    // leadView->setHeight(height());
    // leadView->setPositionRole(cwScrap::LeadPosition);
    // // leadView->setTransformUpdater(TransformUpdater);
    // leadView->setSelectionManager(SelectionMananger);
    // leadView->setScrap(scrap);

    // ScrapToView.insert(scrap, leadView);
}

/**
 * @brief cwLeadView::removeScrap
 * @param scrap - Removes the scrap from the lead view
 */
void cwLeadView::removeScrap(cwScrap *scrap)
{
    Q_ASSERT(scrap != nullptr);

    auto items = m_leadItems.value(scrap);
    for(auto item : items) {
        item->deleteLater();
    }
    m_leadItems.remove(scrap);


//    Q_ASSERT(ScrapToView.contains(scrap));

    // if(ScrapToView.contains(scrap)) {
    //     cwScrapLeadView* view = ScrapToView.value(scrap);
    //     view->deleteLater();
    // }
}

QQuickItem* cwLeadView::createItem()
{
    if(m_itemComponent == nullptr) {
        createComponent();
        // qDebug() << "ItemComponent hasn't been created, call createComponent(). THIS IS A BUG" << LOCATION;
        // return nullptr;
    }

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QQuickItem* item = qobject_cast<QQuickItem*>(m_itemComponent->create(context));
    if(item == nullptr) {
        qDebug() << "Problem creating new point item ... " << qmlSource() << "Didn't compile. THIS IS A BUG!" << LOCATION;
        qDebug() << "Compiling errors:" << m_itemComponent->errorString();
        return nullptr;
    }

    //Reparent the item so the reverse transform works correctly
    //This will keep the item's size and rotation consistent
    item->setParentItem(this);
    item->setParent(this);
    // Q_ASSERT(item->parent() == this);

    //Add the point to the transform updater
    if(TransformUpdater != nullptr) {
        TransformUpdater->addPointItem(item);
    } else {
        qDebug() << "No transformUpdater, point's won't be positioned correctly, this is a bug" << LOCATION;
    }

    return item;
}

void cwLeadView::createComponent()
{
    //Make sure we have a note component so we can create it
    if(m_itemComponent == nullptr) {
        QQmlContext* context = QQmlEngine::contextForObject(this);
        if(context == nullptr) {
            qDebug() << "Context is nullptr, did you forget to set the context? THIS IS A BUG" << LOCATION;
            return;
        }

        m_itemComponent = new QQmlComponent(context->engine(), qmlSource(), this);
        if(m_itemComponent->isError()) {
            qDebug() << "Point errors:" << m_itemComponent->errorString();
        }
    }
}

QString cwLeadView::qmlSource() const
{
    return QStringLiteral("qrc:/cavewherelib/cavewherelib/LeadPoint.qml");
}

/**
 * @brief cwLeadView::scrapsAdded
 * @param parent
 * @param begin
 * @param end
 *
 * This should be called when the model adds scraps
 */
void cwLeadView::scrapsAdded(QModelIndex parent, int begin, int end)
{
    if(RegionModel->isNote(parent)) {
        for(int i = begin; i <= end; i++) {
            //Get the scrap out of the model
            QModelIndex index = RegionModel->index(i, 0, parent);
            cwScrap* scrap = RegionModel->data(index, cwRegionTreeModel::ObjectRole).value<cwScrap*>();

            //Add the scrap
            addScrap(scrap);
        }
    }
}

/**
 * @brief cwLeadView::scrapsRemoved
 * @param parent
 * @param begin
 * @param end
 *
 * This should be called when the model removes scraps
 */
void cwLeadView::scrapsRemoved(QModelIndex parent, int begin, int end)
{
    if(RegionModel->isNote(parent)) {
        for(int i = begin; i <= end; i++) {
            //Get the scrap out of the model
            QModelIndex index = RegionModel->index(i, 0, parent);
            cwScrap* scrap = RegionModel->data(index, cwRegionTreeModel::ObjectRole).value<cwScrap*>();

            //Add the scrap
            removeScrap(scrap);
        }
    }
}


/**
* @brief cwLeadView::camera
* @return The camera that the lead view uses to calculate the lead positions
*/
cwCamera* cwLeadView::camera() const {
    return TransformUpdater->camera();
}

/**
* @brief cwLeadView::setCamera
* @param camera - The camera for the LeadView
*
* The camera allows the leadView to translate the lead positions into 2D space
*/
void cwLeadView::setCamera(cwCamera* camera) {
    TransformUpdater->setCamera(camera);
}

/**
 * @brief cwLeadView::select
 * @param scarp - The scrap that the lead is located in
 * @param index - The index of the lead in the scrap
 *
 * This finds the scrapView for the scrap and selects the lead at index. If the scrap
 * isn't in the view, this does nothing.
 */
void cwLeadView::select(cwScrap *scrap, int index)
{
    if(m_leadItems.contains(scrap)) {
        auto items = m_leadItems.value(scrap);
        if(index < items.size()) {
            auto item = items.at(index);
            SelectionMananger->setSelectedItem(item);
        }
    }
}
