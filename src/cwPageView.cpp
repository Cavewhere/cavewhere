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
#include <QStack>

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
 *
 * This function should be called when the PageSelectionModel's currentPage has been changed
 * This will update the view's current page with the PageSelectionModel's current page.
 */
void cwPageView::loadCurrentPage()
{
    Q_ASSERT(!PageSelectionModel.isNull());

    cwPage* currentPage = PageSelectionModel->currentPage();

    if(currentPage != nullptr && currentPage->component() != nullptr) {

        QQuickItem* item = ComponentToItem.value(currentPage->component());
        if(item == nullptr) {
            item = createChildItemFromComponent(currentPage->component(), currentPage);
            ComponentToItem.insert(currentPage->component(), item);
        } else {
            //Update all the properties for the page, the parent pages
            QStack<cwPage*> parentPages;
            cwPage* stackCurrent = currentPage;
            while(stackCurrent != nullptr) {
                parentPages.push(stackCurrent);
                stackCurrent = stackCurrent->parentPage();
            }

            while(!parentPages.isEmpty()) {
                cwPage* page = parentPages.pop();
                QQuickItem* pageItem = ComponentToItem.value(page->component());

                if(pageItem != nullptr) {
                    updateProperties(pageItem, page);
                }
            }
        }

        //Show the page
        showPage(item);
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
 * @param pageItem - The pageItem who's properties will be updated
 * @param page - The page, with it's properties that will update page
 *
 * This goes through all the properties and updates them in the pageItem
 */
void cwPageView::updateProperties(QQuickItem *pageItem, cwPage* page)
{
    Q_ASSERT(pageItem != nullptr);

    if(page != nullptr) {
        cwPageViewAttachedType* attachedType = qobject_cast<cwPageViewAttachedType*>(
                    qmlAttachedPropertiesObject<cwPageView>(pageItem));

        Q_ASSERT(attachedType != nullptr);

        attachedType->setPage(page);
        QVariantMap properties = attachedType->defaultProperties();
        QVariantMap pageProperties = page->pageProperties();

        //Merge the default with the page properties, the page properties overwrite
        //the default properties
        foreach (QString propertyName, pageProperties.keys()) {
            properties.insert(propertyName, pageProperties.value(propertyName));
        }

        //Go through all the properties and set them on the item
        foreach(QString propertyName, properties.keys()) {
            QByteArray propertyNameBytes = propertyName.toLocal8Bit();

            if(pageItem->metaObject()->indexOfProperty(propertyNameBytes.data()) != -1) {
                qDebug() << "Setting property:" << pageItem << page->fullname() << propertyNameBytes << properties.value(propertyName);
                pageItem->setProperty(propertyNameBytes.data(), properties.value(propertyName));
            } else {
                qDebug() << "Property" << propertyName << "doesn't exist for " << pageItem << ", this is a bug" << LOCATION;
            }
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
 * @brief cwPageView::childItemFromComponent
 * @param component
 * @return Return's the created quick item from component
 */
QQuickItem *cwPageView::createChildItemFromComponent(QQmlComponent *component, cwPage* page)
{
    component->setParent(this);

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QObject* object = component->beginCreate(context);
    object->setParent(this);

    Q_ASSERT(dynamic_cast<QQuickItem*>(object) != nullptr);
    QQuickItem* item = static_cast<QQuickItem*>(object);
    item->setParentItem(this);
    PageItems.append(item);

    //For this to work propertly, we set the properties before the component has been complete
    updateProperties(item, page);

    component->completeCreate();

    //Update the properties after the component has been completed
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
* @return Returns the component that is used to create the Unknown page
*/
QQmlComponent* cwPageView::unknownPageComponent() const {
    return UnknownPageComponent;
}

/**
* @brief cwPageSelectionModel::setUnknownPageComponent
* @param unknownPageComponent - The unknown page component
*
* This is used to create the unknown page item. The unknown page item is shown when
* PageSelectionModel changes the current page to null. The view will show the unknown page
* at that point.
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
