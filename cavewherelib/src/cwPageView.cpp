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
            disconnect(PageSelectionModel.data(), &cwPageSelectionModel::currentPageChanged,
                       this, &cwPageView::loadCurrentPage);
        }

        PageSelectionModel = pageSelectionModel;

        if(!PageSelectionModel.isNull()) {
            connect(PageSelectionModel.data(), &cwPageSelectionModel::currentPageChanged,
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
    if(currentPage != CurrentPage && currentPage != PageSelectionModel->rootPage()) {

        if(!CurrentPage.isNull()) {
            disconnect(CurrentPage.data(), &cwPage::selectionPropertiesChanged,
                       this, &cwPageView::updateSelectionOnCurrentPage);
        }

        CurrentPage = currentPage;

        if(currentPage != nullptr && currentPage->component() != nullptr) {

            connect(CurrentPage.data(), &cwPage::selectionPropertiesChanged,
                    this, &cwPageView::updateSelectionOnCurrentPage);

            QQuickItem* item = pageItem(CurrentPage);

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
}

/**
 * @brief cwPageView::updateSelection
 *
 * This update's the selection properties on the current page
 */
void cwPageView::updateSelectionOnCurrentPage()
{
    Q_ASSERT(CurrentPage == PageSelectionModel->currentPage());
    Q_ASSERT(CurrentPage != nullptr);
    QQuickItem* pageItem = ComponentToItem.value(CurrentPage->component(), nullptr);

    if(pageItem != nullptr) {
        cwPageViewAttachedType* attachedType = qobject_cast<cwPageViewAttachedType*>(
                    qmlAttachedPropertiesObject<cwPageView>(pageItem));

        Q_ASSERT(attachedType != nullptr);

        updateProperties(pageItem,
                         attachedType->defaultSelectionProperties(),
                         CurrentPage->selectionProperties());
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

        updateProperties(pageItem, attachedType->defaultProperties(), page->pageProperties());
        updateProperties(pageItem, attachedType->defaultSelectionProperties(), page->selectionProperties());
    }
}

void cwPageView::updateProperties(QQuickItem *pageItem, QVariantMap defaultProperties, QVariantMap pageProperties)
{
    Q_ASSERT(pageItem != nullptr);
    //Merge the default with the page properties, the page properties overwrite
    //the default properties
    foreach (QString propertyName, pageProperties.keys()) {
        defaultProperties.insert(propertyName, pageProperties.value(propertyName));
    }

    //Go through all the properties and set them on the item
    foreach(QString propertyName, defaultProperties.keys()) {
        QByteArray propertyNameBytes = propertyName.toLocal8Bit();

        if(pageItem->metaObject()->indexOfProperty(propertyNameBytes.data()) != -1) {
//            qDebug() << "Setting property:" << pageItem << propertyNameBytes << defaultProperties.value(propertyName);
            pageItem->setProperty(propertyNameBytes.data(), defaultProperties.value(propertyName));
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

    emit currentPageItemChanged();
}

/**
 * @brief cwPageView::childItemFromComponent
 * @param component
 * @return Return's the created quick item from component
 */
QQuickItem *cwPageView::createChildItemFromComponent(QQmlComponent *component, cwPage* page)
{
//    component->setParent(this);

    QQmlContext* context = QQmlEngine::contextForObject(this);
    QObject* object = component->beginCreate(context);

    if (!object) {
        // If creation failed, print out the errors
        const QList<QQmlError> errors = component->errors();
        for (const QQmlError &error : errors) {
            qWarning() << "cwPageView:" << error.toString();
        }
    }

    Q_ASSERT(dynamic_cast<QQuickItem*>(object) != nullptr); //If this fails, there's a qml error
    QQuickItem* item = static_cast<QQuickItem*>(object);
    item->setParentItem(this);
    item->setParent(this);
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


/**
* @brief cwPageView::currentPageItem
* @return The current visible page that the page view is showing
*/
QQuickItem* cwPageView::currentPageItem() const {
    if(!CurrentPage.isNull()) {
        QQuickItem* pageItem = ComponentToItem.value(CurrentPage->component(), nullptr);
        return pageItem;
    }
    return nullptr;
}

QQuickItem *cwPageView::pageItem(cwPage *page)
{
    QQuickItem* item = ComponentToItem.value(page->component());
    if(item == nullptr) {
        item = createChildItemFromComponent(page->component(), page);
        ComponentToItem.insert(page->component(), item);
    } else {
        //Update all the properties for the page, the parent pages
        QStack<cwPage*> parentPages;
        cwPage* stackCurrent = page;
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
    return item;
}

