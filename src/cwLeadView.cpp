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

//Qt includes
#include <QQmlEngine>
#include <QQmlContext>

cwLeadView::cwLeadView(QQuickItem *parent) :
    QQuickItem(parent),
    TransformUpdater(new cwTransformUpdater(this)),
    SelectionMananger(new cwSelectionManager(this))
{
    connect(TransformUpdater, &cwTransformUpdater::cameraChanged, this, &cwLeadView::cameraChanged);

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
    Q_ASSERT(!ScrapToView.contains(scrap));

    QQmlContext* context = QQmlEngine::contextForObject(this);
    cwScrapLeadView* leadView = new cwScrapLeadView(this);
    QQmlEngine::setContextForObject(leadView, context);
    leadView->setWidth(width());
    leadView->setHeight(height());
    leadView->setPositionRole(cwScrap::LeadPosition);
    leadView->setTransformUpdater(TransformUpdater);
    leadView->setSelectionManager(SelectionMananger);
    leadView->setScrap(scrap);

    ScrapToView.insert(scrap, leadView);
}

/**
 * @brief cwLeadView::removeScrap
 * @param scrap - Removes the scrap from the lead view
 */
void cwLeadView::removeScrap(cwScrap *scrap)
{
    Q_ASSERT(scrap != nullptr);
    Q_ASSERT(ScrapToView.contains(scrap));

    cwScrapLeadView* view = ScrapToView.value(scrap);
    view->deleteLater();
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
    if(ScrapToView.contains(scrap)) {
        cwScrapLeadView* scrapLeadView = ScrapToView.value(scrap, nullptr);
        if(scrapLeadView != nullptr) {
            scrapLeadView->setSelectedItemIndex(index);
        }
    }
}
