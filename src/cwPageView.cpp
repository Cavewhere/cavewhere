/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPageView.h"
#include "cwPageSelectionModel.h"
#include "cwPage.h"
#include "cwDebug.h"

//Qt includes
#include <QQmlEngine>

cwPageView::cwPageView(QQuickItem *parentItem) :
    QQuickItem(parentItem)
{

}

cwPageView::~cwPageView()
{

}

/**
* @brief cwPageView::setPageSelectionModel
* @param pageSelectionModel
*
* This sets the page selection model that this view is going to visualize
*/
void cwPageView::setPageSelectionModel(cwPageSelectionModel* pageSelectionModel) {
    if(PageSelectionModel != pageSelectionModel) {

        if(!PageSelectionModel.isNull()) {
            disconnect(PageSelectionModel, &cwPageSelectionModel::currentPageChanged,
                       this, &cwPageView::loadCurrentPage);
        }

        PageSelectionModel = pageSelectionModel;

        if(!PageSelectionModel.isNull()) {
            connect(PageSelectionModel, &cwPageSelectionModel::currentPageChanged,
                       this, &cwPageView::loadCurrentPage);

            loadCurrentPage();
        }

        emit pageSelectionModelChanged();
    }
}

/**
 * @brief cwPageView::loadCurrentPage
 */
void cwPageView::loadCurrentPage()
{
    Q_ASSERT(!PageSelectionModel.isNull());

    cwPage* currentPage = PageSelectionModel->currentPage();

    if(currentPage != nullptr && currentPage->component() != nullptr) {

        QQuickItem* pageItem = ComponentToItem.value(currentPage->component());
        if(pageItem == nullptr) {
            //Create the page item from the component
            QQmlContext* context = QQmlEngine::contextForObject(this);
            QObject* object = currentPage->component()->create(context);
            Q_ASSERT(dynamic_cast<QQuickItem*>(object) != nullptr);
            pageItem = static_cast<QQuickItem*>(object);
            pageItem->setParentItem(this);

            PageSelectionModel->registerQuickItemToPage(pageItem, currentPage);
            ComponentToItem.insert(currentPage->component(), pageItem);
            PageItems.append(pageItem);
        }

        //Update all the properties for the page
        updateProperties(pageItem, currentPage->pageProperties());

        //Show the page
        showPage(pageItem);
    }
}

/**
 * @brief cwPageView::updateProperties
 * @param pageItem
 * @param properties
 *
 * This goes through all the properties and updates them in the pageItem
 */
void cwPageView::updateProperties(QQuickItem *pageItem, QVariantMap properties)
{
    Q_ASSERT(pageItem != nullptr);

    foreach(QString propertyName, properties.keys()) {
        QByteArray propertyNameBytes = propertyName.toLocal8Bit();

        if(pageItem->metaObject()->indexOfProperty(propertyNameBytes.data()) != -1) {
            pageItem->setProperty(propertyNameBytes.data(), properties.value(propertyName));
        } else {
            qDebug() << "Property" << propertyName << "doesn't exist for " << pageItem << ", this is a bug" << LOCATION;
        }
    }
}

/**
 * @brief cwPageView::showPage
 * @param pageItem
 *
 * This hides all the other pages, while showing pageItem
 */
void cwPageView::showPage(QQuickItem *pageItem)
{
    pageItem->setVisible(true);
    foreach(QQuickItem* item, PageItems) {
        if(item != pageItem) {
            item->setVisible(false);
        }
    }
}

/**
* @brief cwPageView::pageSelectionModel
* @return Returns the current selection model
*/
cwPageSelectionModel* cwPageView::pageSelectionModel() const {
    return PageSelectionModel;
}

