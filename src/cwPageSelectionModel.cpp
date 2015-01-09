/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwPageSelectionModel.h"
#include "cwDebug.h"
#include "cwPage.h"

//Qt includse
#include <QDebug>
#include <QQmlComponent>

cwPageSelectionModel::cwPageSelectionModel(QObject *parent) :
    QObject(parent),
    RootPage(new cwPage()),
    LockHistory(false),
    CurrentPage(RootPage),
    CurrentPageIndex(0)
{
    RootPage->setParent(this);
    QuickItemToPage.insert(nullptr, RootPage);

}

/**
 * @brief cwPageSelectionModel::registerQuickItemToPage
 * @param pageObject
 * @param page
 *
 * This will map a pageItem and page together. This function should only be called by a PageView.
 */
void cwPageSelectionModel::registerQuickItemToPage(QQuickItem *pageItem, cwPage *page)
{
    Q_ASSERT(pageItem != nullptr);
    Q_ASSERT(page != nullptr);
    QuickItemToPage.insert(pageItem, page);

    //Sometimes qml loads things out of order and pages must be register when the quickItemToPage
    //is registered.
    if(DelayRegisterationPages.contains(pageItem)) {
        DelayedPage delayedPage = DelayRegisterationPages.value(pageItem);
        Q_ASSERT(delayedPage.ParentPageItem == pageItem);
        registerPage(delayedPage.ParentPageItem,
                     delayedPage.PageName,
                     delayedPage.PageComponent,
                     delayedPage.PageProperties);
        DelayRegisterationPages.remove(pageItem);
    }

}


/**
 * @brief cwPageSelectionModel::registerPage
 * @param parentPageItem
 * @param pageName
 * @param pageComponent
 * @param pageProperties
 * @return
 */
cwPage* cwPageSelectionModel::registerPage(QQuickItem *parentPageItem,
                                           QString pageName,
                                           QQmlComponent *pageComponent,
                                           QVariantMap pageProperties)
{

    Q_ASSERT(!pageName.isEmpty());
    Q_ASSERT(pageComponent != nullptr);

    //The parent page has been register, or this is root page
    if(QuickItemToPage.contains(parentPageItem) || parentPageItem == nullptr) {
        cwPage* parentPage = QuickItemToPage.value(parentPageItem);
        cwPage* page = new cwPage(parentPage, pageComponent, pageName, pageProperties);

        return page;
    } else {
        //The page isn't ready for pages to be registered. Delay registeration.
        //Basically the page is in the middle of being load. Component.onComplete() hasn't
        //been called yet.
        DelayRegisterationPages.insert(parentPageItem, DelayedPage(parentPageItem,
                                                                   pageName,
                                                                   pageComponent,
                                                                   pageProperties));
    }

    qDebug() << "Can't find parent page item:" << parentPageItem << LOCATION;
    return nullptr;
}




/**
 * @brief cwPageSelectionModel::registerRootPage
 * @param page
 *
 * Sets the root page for the page selection model. This will reset all the data in the model
 *
 * This will not clear the orphen list
 */
//void cwPageSelectionModel::registerRootPage(QObject *page)
//{
//    if(RootPage != page) {
//        RootPage = page;

//        //Clear all the data in the selection model
//        PageParameters.clear();
//        PageDefaults.clear();
//        RootNode = PageTreeNodePtr(new PageTreeNode());
//        ObjectToPageLookup.clear();
//    }
//}

/**
 * @brief cwPageSelectionModel::registerDefaultPageSelection
 * @param page
 * @param function
 * @param defaultSelection
 */
//void cwPageSelectionModel::registerDefaultPageSelection(QObject *page,
//                                                        QByteArray function,
//                                                        QVariantMap defaultSelection)
//{
//    PageDefaultSelection pageDefault;
//    pageDefault.Page = page;
//    pageDefault.Function = function;
//    pageDefault.Parameters = defaultSelection;

//    PageDefaults.insert(page, pageDefault);
//}

/**
 * @brief cwPageSelectionModel::registerPage
 * @param object
 * @param name
 * @param function
 * @param parameters
 */
//int cwPageSelectionModel::registerPage(QObject *from,
//                                       QString pageName,
//                                       QQmlComponent* pageComponent,
//                                       QVariantMap pageProperties)
//{
//    PageLink link;
//    link.FromObject = from;
//    link.PageProperties = pageProperties;
//    link.Name = pageName;
//    link.Component = pageComponent;

//    insertIntoPageTree(link);



////    PageLinks.insert(to, link);
////    qDebug() << "registerPageLink:" << objectToLinkStringList(to).join("/") << to;
////    StringToPageLinks.insert(objectToLinkStringList(to).join("/"), link);
//}

/**
 * @brief cwPageSelectionModel::registerSelection
 * @param object
 * @param function
 */
//void cwPageSelectionModel::registerSelection(QObject *object, QByteArray function)
//{
//    PageParameter pageSelectionParameters;
//    pageSelectionParameters.Object = object;
//    pageSelectionParameters.Function = function;

//    PageParameters.insert(object, pageSelectionParameters);
//}

/**
 * @brief cwPageSelectionModel::setCurrentSelection
 * @param object
 * @param parameters
 */
//void cwPageSelectionModel::setCurrentSelection(QObject *object, QVariantMap parameters)
//{
//    Q_UNUSED(object);
//    Q_UNUSED(parameters);



//}

/**
* @brief cwPageSelectionModel::gotoPage
* @param currentPage - The current page
*
* This will update the current page of the model.
*/
void cwPageSelectionModel::setCurrentLink(QString link) {
    if(link.isEmpty()) {
        return;
    }

    if(currentLink() != link) {
        cwPage* page = stringToPage(link);

        gotoPage(page);
    }
}

/**
 * @brief cwPageSelectionModel::gotoPage
 * @param page
 */
void cwPageSelectionModel::gotoPage(cwPage *page)
{
    Q_ASSERT(page != nullptr);

    if(CurrentPage != page) {

        if(CurrentPage != nullptr) {
            disconnect(CurrentPage, &cwPage::nameChanged, this, &cwPageSelectionModel::currentLinkChanged);
        }

        CurrentPage = page;

        if(CurrentPage != nullptr) {
            connect(CurrentPage, &cwPage::nameChanged, this, &cwPageSelectionModel::currentLinkChanged);
        }

        if(!LockHistory) {
            //Remove old pages
            int numberToRemove = (PageHistory.size() - 1) - CurrentPageIndex;
            for(int i = 0; i < numberToRemove; i++) {
                PageHistory.removeLast();
            }

            //Set the new current page
            PageHistory.append(CurrentPage);
            CurrentPageIndex = PageHistory.size() - 1;
        }

        emit currentPageChanged();
        emit currentLinkChanged();
    }
}

/**
 * @brief cwPageSelectionModel::setCurrentPage
 * @param parent
 * @param pageLink
 */
void cwPageSelectionModel::gotoPageByName(QQuickItem *parentItem, QString subPage)
{
    if(QuickItemToPage.contains(parentItem)) {
        cwPage* parentPage = QuickItemToPage.value(parentItem);
        cwPage* childPage = parentPage->childPage(subPage);

        if(childPage != nullptr) {
            gotoPage(childPage);
        } else {
            qDebug() << "Child page is null" << parentItem << subPage << "This is a bug!" << LOCATION;
        }
    } else {
        qDebug() << "Can't go to page:" << parentItem << subPage << "because parentItem isn't registered" << LOCATION;
    }
}

/**
 * @brief cwPageSelectionModel::back
 */
void cwPageSelectionModel::back()
{
    if(hasBackward()) {
        LockHistory = true;

        CurrentPageIndex--;
        Q_ASSERT(CurrentPageIndex < PageHistory.size());
        Q_ASSERT(CurrentPageIndex >= 0);

        gotoPage(PageHistory.at(CurrentPageIndex));

        LockHistory = false;
    }
}

/**
 * @brief cwPageSelectionModel::forward
 */
void cwPageSelectionModel::forward()
{
    if(hasForward()) {
        LockHistory = true;

        CurrentPageIndex++;
        Q_ASSERT(CurrentPageIndex < PageHistory.size());
        Q_ASSERT(CurrentPageIndex >= 0);

        gotoPage(PageHistory.at(CurrentPageIndex));

        LockHistory = false;
    }
}

///**
// * @brief cwPageSelectionModel::reload
// *
// * This will reload the current page.
// */
//void cwPageSelectionModel::reload()
//{
//    loadPage(CurrentPage, QString());
//}

/**
 * @brief cwPageSelectionModel::pageDifferance
 * @param newPage
 * @param oldPages
 * @return The differance of pages between the newPage and the oldPage
 *
 * If old page is greater in size than newPage, this will return all of the newPage's pages. If old
 * page and newPageLink are equal or less in size, then this will return new pages in newPageLink.
 *
 */
//QStringList cwPageSelectionModel::pageLinkDifferance(QString newPageLink, QString oldPageLink) const
//{
//    Q_ASSERT(newPageLink != oldPageLink);

//    QStringList expandedNewLinks = expandLink(newPageLink);
//    QStringList expandedOldLinks = expandLink(oldPageLink);

//    if(expandedOldLinks.size() < expandedOldLinks.size()) {
//        int i = 0;
//        for(; i < expandedOldLinks.size(); i++) {
//            const QString& newSubLink = expandedNewLinks.at(i);
//            const QString& oldSubLink = expandedOldLinks.at(i);

//            if(newSubLink != oldSubLink) {
//                break;
//            }
//        }

//        if(i == expandedOldLinks.size()) {
//            Q_ASSERT(i < expandedNewLinks.size());

//            //The new link is a sub page, so only return the subpage links
//            QStringList subLinks;
//            for(; i < expandedNewLinks.size(); i++) {
//                subLinks.append(expandedNewLinks.at(i));
//            }

//            return subLinks;
//        }
//    }
//    //Return the fully expanded new links
//    return expandedNewLinks;
//}

/**
 * @brief cwPageSelectionModel::objectToLinkStringList
 * @param object
 * @return A link string for the object, this is useful for calling setCurrentPage
 */
//QStringList cwPageSelectionModel::objectToLinkStringList(QObject *object) const
//{
//    QStringList linkString;
//    while(ObjectToPageLookup.contains(object)) {
//        PageTreeNodePtr node = ObjectToPageLookup.value(object);
//        linkString.prepend(node->Link.Name);
//        object = node->Link.FromObject;
//    }
//    return linkString;
//}

/**
 * @brief cwPageSelectionModel::expandLink
 * @param link - The link that will be expanded
 * @return A list of expanded links
 *
 * For example:
 *
 * link = Data/Cave A/Trip 1
 *
 * This will expand to:
 *
 * [0] = Data
 * [1] = Data/Cave A
 * [2] = Data/Cave A/Trip 1
 */
//QStringList cwPageSelectionModel::expandLink(QString link) const
//{
//    QStringList linkParts = link.split("/");

//    QStringList expandedLinks;
//    expandedLinks.reserve(3);

//    //Populate the expandedLinks with empty strings
//    for(int i = 0; i < linkParts.size(); i++) {
//        expandedLinks.append(QString());
//    }

//    for(int i = 0; i < linkParts.size(); i++) {
//        QString linkPart = linkParts.at(i);

//        if(i != 0) {
//            linkPart = QString("/") + linkPart;
//        }

//        for(int j = i; j < linkParts.size(); j++) {
//            expandedLinks[j] = expandedLinks.at(j) + linkPart;
//        }
//    }

//    return expandedLinks;
//}

/**
 * @brief cwPageSelectionModel::printPageHistory
 *
 * This function is for debugging. It prints the page history
 */
void cwPageSelectionModel::printPageHistory() const
{
    qDebug() << "----------------- Page History ----------------------";
    for(int i = 0; i < PageHistory.size(); i++) {
        cwPage* page = PageHistory.at(i);
        if(i == CurrentPageIndex) {
            qDebug() << "[" << i << "] -" << page->fullname() << " <- Current";
        } else {
            qDebug() << "[" << i << "] -" << page->fullname();
        }
    }
    qDebug() << "*****************************************************";
}

/**
 * @brief cwPageSelectionModel::loadPage
 * @param newPage
 * @param currentPage
 *
 * This is a helper function for the reload() and setCurrentPage(). This function calls the qml
 * to update the current page.
 */
//void cwPageSelectionModel::loadPage(QString newPage, QString oldPage)
//{
//    CurrentPage = stringToPage(newPage);


//    QStringList newPageList = pageLinkDifferance(newPage, oldPage);
//    foreach(QString pageString, newPageList) {
//        //Go to the page
//        Page* page = stringToPage(pageString);

//    }
//        if(!page.FromObject.isNull()) {
//            bool okay = QMetaObject::invokeMethod(page.FromObject,
//                                                  page.Function,
//                                                  Q_ARG(QVariant, page.Parameters));
//            if(!okay) {
//                qDebug() << "Can't invokeMethod" << page.FromObject << page.Function << page.Parameters << LOCATION;
//            }
//        } else {
//            qDebug() << "Link parent object is null for " << pageString << "this is a bug" << LOCATION;
//        }
//    }

//    if(!newPageList.isEmpty()) {
//        //Load the default view for the page
//        PageLink link = stringToPageLink(newPage);
//        PageDefaultSelection pageDefault = PageDefaults.value(link.ToObject);
//        qDebug() << "PageDefault:" << newPage << pageDefault.Page << pageDefault.Function;
//        if(!pageDefault.Page.isNull()) {
//            bool okay = QMetaObject::invokeMethod(pageDefault.Page,
//                                                  pageDefault.Function,
//                                                  Q_ARG(QVariant, pageDefault.Parameters));
//            if(!okay) {
//                qDebug() << "Can't invokeMethod" << pageDefault.Page << link.Function << link.Parameters << LOCATION;
//            }
//        }
//    }

//    if(!LockHistory) {
//        //Remove old pages
//        int numberToRemove = (PageHistory.size() - 1) - CurrentPageIndex;
//        for(int i = 0; i < numberToRemove; i++) {
//            PageHistory.removeLast();
//        }

//        //Set the new current page
//        PageHistory.append(newPage);
//        CurrentPageIndex = PageHistory.size() - 1;
//    }
//}

/**
 * @brief cwPageSelectionModel::instertIntoPageTree
 * @param link
 *
 * This inserts the link into the page tree. Links can be inserted out of order. If inserted out of
 * order, ie link is register before parent pages, this will orphen the link and print a warning messsage.
 *
 * This function will then try to instert orphen links.
 */
//void cwPageSelectionModel::insertIntoPageTree(const cwPageSelectionModel::PageLink &link, bool appendToOrphenLinks)
//{
//    PageTreeNodePtr parentNode;

//    PageTreeNodePtr childNode(new PageTreeNode());
//    childNode->Link = link;

//    //The from object node already exists in the tree
//    if(ObjectToPageLookup.contains(link.FromObject)) {
//        //Add the link as a child of the fromObject
//        parentNode = ObjectToPageLookup.value(link.FromObject);
//    } else {
//        //From object doesn't exist in the tree
//        Q_ASSERT(!RootPage.isNull()); //Root page has been deleted or not set, via registerRootPage
//        if(link.FromObject == RootPage) {
//            //Add to RootNode
//            parentNode = RootNode;
//        } else {
//            //This is an orphin link
//            if(appendToOrphenLinks) {
//                qDebug() << "Found and orphened link" << link.Name << "To:" << link.ToObject << "From:" << link.FromObject << LOCATION;
//                OrphenLinks.insert(link, 0);
//            }
//            return;
//        }
//    }

//    //Add the child to the parentNode
//    OrphenLinks.remove(link);
//    parentNode->ChildNodes.insert(link.Name, childNode);
//    ObjectToPageLookup.insert(link.ToObject, childNode);

//    foreach(const PageLink& link, OrphenLinks.keys()) {
//        //Recursive call for trying to append orphen links,
//        insertIntoPageTree(link, false);
//    }
//}

/**
 * @brief cwPageSelectionModel::stringToPageLink
 * @param pageLinkString
 * @return
 *
 * This will iterate the through the pageTree starting from the RootNode and find the link base
 * on the string. The string will be broken into it's parents using "/" as a seperator. It's part
 * is used to traverse the tree.
 *
 * If the link can't be found, then this returns null page
 */
cwPage* cwPageSelectionModel::stringToPage(const QString &pageLinkString) const
{
    QStringList parts = cwPage::splitLinkIntoParts(pageLinkString);
    cwPage* current = RootPage;

    foreach(QString part, parts) {
        cwPage* nextPage = current->childPage(part);
        if(nextPage == nullptr) {
            return nullptr;
        }
        current = nextPage;
    }

    return current;
}

/**
* @brief cwPageSelectionModel::currentPage
* @return Retruns the full name of the current page
*
* You can use this to call gotoPage()
*/
QString cwPageSelectionModel::currentLink() const {
    return CurrentPage->fullname();
}

