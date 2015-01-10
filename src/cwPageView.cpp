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
#include "cwPageViewAttachedType.h"
#include "cwDebug.h"

//Qt includes
#include <QQmlEngine>
#include <QQmlContext>

cwPageView::cwPageView(QQuickItem *parentItem) :
    QQuickItem(parentItem),
    UnknownPageItem(nullptr)
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
            pageItem = createChildItemFromComponent(currentPage->component(), currentPage);
//            PageSelectionModel->registerQuickItemToPage(pageItem, currentPage);
            ComponentToItem.insert(currentPage->component(), pageItem);
        } else {
            //Update all the properties for the page
            updateProperties(pageItem, currentPage);
        }

        //Show the page
        showPage(pageItem);
    } else {
        if(UnknownPageItem != nullptr) {
            showPage(UnknownPageItem);
        } else {
            qDebug() << "Couldn't show unknown page!!! This is a bug" << LOCATION;
        }
    }
}

/**
 * @brief cwPageView::updateProperties
 * @param pageItem
 * @param properties
 *
 * This goes through all the properties and updates them in the pageItem
 */
void cwPageView::updateProperties(QQuickItem *pageItem, cwPage* page)
{
    Q_ASSERT(pageItem != nullptr);

    if(page != nullptr) {

        QVariantMap properties = page->pageProperties();
        foreach(QString propertyName, properties.keys()) {
            QByteArray propertyNameBytes = propertyName.toLocal8Bit();

            if(pageItem->metaObject()->indexOfProperty(propertyNameBytes.data()) != -1) {
                qDebug() << "Setting page Properties:" << pageItem << propertyName << properties.value(propertyName);
                pageItem->setProperty(propertyNameBytes.data(), properties.value(propertyName));
            } else {
                qDebug() << "Property" << propertyName << "doesn't exist for " << pageItem << ", this is a bug" << LOCATION;
            }
        }

        cwPageViewAttachedType* attachedType = qobject_cast<cwPageViewAttachedType*>(
                    qmlAttachedPropertiesObject<cwPageView>(pageItem));

        qDebug() << "Setting page properties:" << page << pageItem << attachedType;
        if(attachedType != nullptr) {
            attachedType->setPage(page);
        } else {
            qDebug() << "Can't find attached PageView Object";
        }
    }

//    QQmlContext* context = QQmlEngine::contextForObject(pageItem);
//    context->setContextProperty("page", page);
//    pageItem->setProperty("page", QVariant::fromValue(page));
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
 * @brief cwPageView::childItemFromComponent
 * @param component
 * @return Return's the created quick item from component
 */
QQuickItem *cwPageView::createChildItemFromComponent(QQmlComponent *component, cwPage* page)
{
    QQmlContext* context = QQmlEngine::contextForObject(this);
    QObject* object = component->beginCreate(context);
    object->setParent(this);

    Q_ASSERT(dynamic_cast<QQuickItem*>(object) != nullptr);
    QQuickItem* item = static_cast<QQuickItem*>(object);
    item->setParentItem(this);
    PageItems.append(item);

    updateProperties(item, page);

    component->completeCreate();

    updateProperties(item, page);

    return item;
}

/**
* @brief cwPageView::pageSelectionModel
* @return Returns the current selection model
*/
cwPageSelectionModel* cwPageView::pageSelectionModel() const {
    return PageSelectionModel;
}

/**
* @brief cwPageSelectionModel::unknownPageComponent
* @return
*/
QQmlComponent* cwPageView::unknownPageComponent() const {
    return UnknownPageComponent;
}

/**
* @brief cwPageSelectionModel::setUnknownPageComponent
* @param unknownPageComponent
*/
void cwPageView::setUnknownPageComponent(QQmlComponent* unknownPageComponent) {
    if(UnknownPageComponent != unknownPageComponent) {
        UnknownPageComponent = unknownPageComponent;
        if(UnknownPageItem != nullptr) {
            UnknownPageItem->deleteLater();
        }
        UnknownPageItem = createChildItemFromComponent(UnknownPageComponent, nullptr);
        emit unknownPageComponentChanged();
    }
}
