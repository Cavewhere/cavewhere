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
#include "cwGlobalDirectory.h"

//Qt includse
#include <QDebug>
#include <QQmlComponent>
#include <QVariantMap>

cwPageSelectionModel::cwPageSelectionModel(QObject *parent) :
    QObject(parent),
    RootPage(new cwPage()),
    LockHistory(false),
    CurrentPage(RootPage),
    CurrentPageIndex(0)
{
    RootPage->setParent(this);
    //    QuickItemToPage.insertMulti(nullptr, RootPage);
    //    PageToQuickItem.insert(RootPage, nullptr);
}

/**
 * @brief cwPageSelectionModel::registerQuickItemToPage
 * @param pageObject
 * @param page
 *
 * This will map a pageItem and page together. This function should only be called by a PageView.
 */
//void cwPageSelectionModel::registerQuickItemToPage(QQuickItem *pageItem, cwPage *page)
//{
//    Q_ASSERT(pageItem != nullptr);
//    Q_ASSERT(page != nullptr);
//    qDebug() << "Registering pageItem:" << (void*)pageItem << page << page->name();
//    QuickItemToPage.insertMulti(pageItem, page);
//    PageToQuickItem.insert(page, pageItem);

//    //Sometimes qml loads things out of order and pages must be register when the quickItemToPage
//    //is registered.
//    if(DelayRegisterationPages.contains(pageItem)) {
//        DelayedPage delayedPage = DelayRegisterationPages.value(pageItem);
//        Q_ASSERT(delayedPage.ParentPageItem == pageItem);
//        registerPage(delayedPage.ParentPageItem,
//                     delayedPage.PageName,
//                     delayedPage.PageComponent,
//                     delayedPage.PageProperties);
//        DelayRegisterationPages.remove(pageItem);
//    }

//}


/**
 * @brief cwPageSelectionModel::registerPage
 * @param parentPageItem
 * @param pageName
 * @param pageComponent
 * @param pageProperties
 * @return
 */
cwPage* cwPageSelectionModel::registerPage(cwPage *parentPage,
                                           QString pageName,
                                           QQmlComponent *pageComponent,
                                           QVariantMap pageProperties)
{

    Q_ASSERT(!pageName.isEmpty());
    Q_ASSERT(pageComponent != nullptr);


    if(parentPage == nullptr) {
        parentPage = RootPage;
    }

    qDebug() << "Registering page:" << parentPage << parentPage->name() << pageName << pageComponent << pageProperties;

    Q_ASSERT(isPageOrphan(parentPage)); //Make sure the parent page exists in the model

    cwPage* page = new cwPage(parentPage, pageComponent, pageName, pageProperties);
    return page;

    //    //The parent page has been register, or this is root page
    //    if(QuickItemToPage.contains(parentPageItem) || parentPageItem == nullptr) {
    //        cwPage* parentPage = QuickItemToPage.value(parentPageItem);

    //        return page;
    //    } else {
    //        //The page isn't ready for pages to be registered. Delay registeration.
    //        //Basically the page is in the middle of being load. Component.onComplete() hasn't
    //        //been called yet.
    //        DelayRegisterationPages.insert(parentPageItem, DelayedPage(parentPageItem,
    //                                                                   pageName,
    //                                                                   pageComponent,
    //                                                                   pageProperties));
    //    }

    //    qDebug() << "Can't find parent page item:" << parentPageItem << LOCATION;
    //    return nullptr;
}

/**
 * @brief cwPageSelectionModel::unregisterPage
 * @param page
 */
void cwPageSelectionModel::unregisterPage(cwPage *page)
{
    qDebug() << "unregisterPage:" << page;

    if(page != nullptr) {
        Q_ASSERT(page != currentPage());

        page->parentPage()->removeChild(page);
        page->deleteLater();

        //        QQuickItem* item = PageToQuickItem.value(page);
        //        Q_ASSERT(item != nullptr);
        //        QuickItemToPage.remove(item);
        //        PageToQuickItem.remove(page);
    } else {
        qDebug() << "Unregistering a null page! This is a bug" << LOCATION;
    }
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

        if(page != nullptr) {
            gotoPage(page);
        } else {
            //Don't know how to go to link, perhaps, it's not register yet
            //This will go through all the parent pages until we get to the full
            //link address
            QStringList expandedLinks = expandLink(link);
            for(int i = 0; i < expandedLinks.size(); i++) {
                QString parentLink = expandedLinks.at(i);
                page = stringToPage(parentLink);

                if(page == nullptr) {
                    UnknownLinkAddress = link;
                }

                //Lock the history until we get last element
                if(i != expandedLinks.size() - 1) { LockHistory = true; }

                gotoPage(page);

                if(i != expandedLinks.size() - 1) { LockHistory = false; }

                if(page == nullptr) {
                    //Page doesn't exist
                    break;
                }
            }
        }
    }
}

/**
 * @brief cwPageSelectionModel::gotoPage
 * @param page
 *
 * If the page is null, this will goto the UnknownPage
 */
void cwPageSelectionModel::gotoPage(cwPage *page)
{
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
            if(PageHistory.isEmpty() || CurrentPage != PageHistory.last()) {
                PageHistory.append(CurrentPage);
                CurrentPageIndex = PageHistory.size() - 1;
            }
        }

        printPageHistory();

        emit currentPageChanged();
        emit currentLinkChanged();
    }
}

/**
 * @brief cwPageSelectionModel::setCurrentPage
 * @param parent
 * @param pageLink
 */
void cwPageSelectionModel::gotoPageByName(cwPage *parentPage, QString subPage)
{
    qDebug() << "Go to page:" << parentPage << subPage;

    if(parentPage == nullptr) {
        parentPage = RootPage;
    }

    cwPage* childPage = parentPage->childPage(subPage);

    if(childPage == nullptr) {
        UnknownLinkAddress = QString("%1/%2").arg(parentPage->fullname()).arg(subPage);
    }

    gotoPage(childPage);
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

/**
 * @brief cwPageSelectionModel::createUnknownPage
 *
 * Creates an unknown page. This page is shown when a link is broken or the user types in a bad link
 */
//void cwPageSelectionModel::updateUnknownPage(QString badLink)
//{
//    Q_ASSERT(UnknownPageComponent != nullptr);

//    QVariantMap map;
//    map.insert("link", badLink);

//    if(UnknownPage != nullptr) {
//        UnknownPage->component()->setParent(this);
//        unregisterPage(UnknownPage);
//    }

//    UnknownPage = registerPage(nullptr, "Unknown Page", UnknownPageComponent, map);
//}

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
QStringList cwPageSelectionModel::expandLink(QString link) const
{
    QStringList linkParts = link.split("/");

    QStringList expandedLinks;
    expandedLinks.reserve(linkParts.size());

    //Populate the expandedLinks with empty strings
    for(int i = 0; i < linkParts.size(); i++) {
        expandedLinks.append(QString());
    }

    for(int i = 0; i < linkParts.size(); i++) {
        QString linkPart = linkParts.at(i);

        if(i != 0) {
            linkPart = QString("/") + linkPart;
        }

        for(int j = i; j < linkParts.size(); j++) {
            expandedLinks[j] = expandedLinks.at(j) + linkPart;
        }
    }

    return expandedLinks;
}

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
 * @brief cwPageSelectionModel::isPageOrphan
 * @param page - The page that will be checked
 * @return True if the last parent page is the RootPage. False if page isn't in the selection model
 */
bool cwPageSelectionModel::isPageOrphan(cwPage *page) const
{
    while(page != RootPage && page != nullptr) {
        page = page->parentPage();
    }
    return page == RootPage;
}

/**
* @brief cwPageSelectionModel::currentPage
* @return Retruns the full name of the current page
*
* You can use this to call gotoPage()
*/
QString cwPageSelectionModel::currentLink() const {
    if(CurrentPage != nullptr) {
        return CurrentPage->fullname();
    }
    return UnknownLinkAddress;
}



