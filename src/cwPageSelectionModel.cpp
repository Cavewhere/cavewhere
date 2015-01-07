/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwPageSelectionModel.h"
#include "cwDebug.h"

//Qt includse
#include <QDebug>

cwPageSelectionModel::cwPageSelectionModel(QObject *parent) :
    QObject(parent),
    LockHistory(false),
    CurrentPageIndex(0),
    RootNode(new PageTreeNode())
{

}

/**
 * @brief cwPageSelectionModel::registerRootPage
 * @param page
 *
 * Sets the root page for the page selection model. This will reset all the data in the model
 *
 * This will not clear the orphen list
 */
void cwPageSelectionModel::registerRootPage(QObject *page)
{
    if(RootPage != page) {
        RootPage = page;

        //Clear all the data in the selection model
        PageParameters.clear();
        PageDefaults.clear();
        RootNode = PageTreeNodePtr(new PageTreeNode());
        ObjectToPageLookup.clear();
    }
}

/**
 * @brief cwPageSelectionModel::registerDefaultPageSelection
 * @param page
 * @param function
 * @param defaultSelection
 */
void cwPageSelectionModel::registerDefaultPageSelection(QObject *page,
                                                        QByteArray function,
                                                        QVariantMap defaultSelection)
{
    PageDefaultSelection pageDefault;
    pageDefault.Page = page;
    pageDefault.Function = function;
    pageDefault.Parameters = defaultSelection;

    PageDefaults.insert(page, pageDefault);
}

/**
 * @brief cwPageSelectionModel::registerPage
 * @param object
 * @param name
 * @param function
 * @param parameters
 */
void cwPageSelectionModel::registerPageLink(QObject *from,
                                            QObject *to,
                                            QString name,
                                            QByteArray function,
                                            QVariantMap pageLink)
{
    PageLink link;
    link.FromObject = from;
    link.ToObject = to;
    link.Parameters = pageLink;
    link.Name = name;
    link.Function = function;

    insertIntoPageTree(link);

//    PageLinks.insert(to, link);
//    qDebug() << "registerPageLink:" << objectToLinkStringList(to).join("/") << to;
//    StringToPageLinks.insert(objectToLinkStringList(to).join("/"), link);
}

/**
 * @brief cwPageSelectionModel::registerSelection
 * @param object
 * @param function
 */
void cwPageSelectionModel::registerSelection(QObject *object, QByteArray function)
{
    PageParameter pageSelectionParameters;
    pageSelectionParameters.Object = object;
    pageSelectionParameters.Function = function;

    PageParameters.insert(object, pageSelectionParameters);
}

/**
 * @brief cwPageSelectionModel::setCurrentSelection
 * @param object
 * @param parameters
 */
void cwPageSelectionModel::setCurrentSelection(QObject *object, QVariantMap parameters)
{
    Q_UNUSED(object);
    Q_UNUSED(parameters);



}

/**
* @brief cwPageSelectionModel::setCurrentPage
* @param currentPage - The current page
*
* This will update the current page of the model. This will try to change the selection the current
* page of the qml
*/
void cwPageSelectionModel::setCurrentPage(QString currentPage) {
    if(currentPage.isEmpty()) {
        return;
    }

    if(CurrentPage != currentPage) {

        loadPage(currentPage, CurrentPage);
        CurrentPage = currentPage;

        emit currentPageChanged();
    }
}

/**
 * @brief cwPageSelectionModel::setCurrentPage
 * @param parent
 * @param pageLink
 */
void cwPageSelectionModel::setCurrentPage(QObject *from, QString subPage)
{
    QStringList linkStringList = objectToLinkStringList(from);
    linkStringList.append(subPage);
    setCurrentPage(linkStringList.join("/"));
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

        setCurrentPage(PageHistory.at(CurrentPageIndex));

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

        setCurrentPage(PageHistory.at(CurrentPageIndex));

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
QStringList cwPageSelectionModel::pageLinkDifferance(QString newPageLink, QString oldPageLink) const
{
    Q_ASSERT(newPageLink != oldPageLink);

    QStringList expandedNewLinks = expandLink(newPageLink);
    QStringList expandedOldLinks = expandLink(oldPageLink);

    if(expandedOldLinks.size() < expandedOldLinks.size()) {
        int i = 0;
        for(; i < expandedOldLinks.size(); i++) {
            const QString& newSubLink = expandedNewLinks.at(i);
            const QString& oldSubLink = expandedOldLinks.at(i);

            if(newSubLink != oldSubLink) {
                break;
            }
        }

        if(i == expandedOldLinks.size()) {
            Q_ASSERT(i < expandedNewLinks.size());

            //The new link is a sub page, so only return the subpage links
            QStringList subLinks;
            for(; i < expandedNewLinks.size(); i++) {
                subLinks.append(expandedNewLinks.at(i));
            }

            return subLinks;
        }
    }
    //Return the fully expanded new links
    return expandedNewLinks;
}

/**
 * @brief cwPageSelectionModel::objectToLinkStringList
 * @param object
 * @return A link string for the object, this is useful for calling setCurrentPage
 */
QStringList cwPageSelectionModel::objectToLinkStringList(QObject *object) const
{
    QStringList linkString;
    while(ObjectToPageLookup.contains(object)) {
        PageTreeNodePtr node = ObjectToPageLookup.value(object);
        linkString.prepend(node->Link.Name);
        object = node->Link.FromObject;
    }
    return linkString;
}

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
    expandedLinks.reserve(3);

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
        const QString& link = PageHistory.at(i);
        if(i == CurrentPageIndex) {
            qDebug() << "[" << i << "] -" << link << " <- Current";
        } else {
            qDebug() << "[" << i << "] -" << link;
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
void cwPageSelectionModel::loadPage(QString newPage, QString oldPage)
{
    QStringList newPageList = pageLinkDifferance(newPage, oldPage);
    foreach(QString page, newPageList) {
        //Go to the page
        PageLink link = stringToPageLink(page);
        if(!link.FromObject.isNull()) {
            bool okay = QMetaObject::invokeMethod(link.FromObject,
                                                  link.Function,
                                                  Q_ARG(QVariant, link.Parameters));
            if(!okay) {
                qDebug() << "Can't invokeMethod" << link.FromObject << link.Function << link.Parameters << LOCATION;
            }
        } else {
            qDebug() << "Link parent object is null for " << page << "this is a bug" << LOCATION;
        }
    }

    if(!newPageList.isEmpty()) {
        //Load the default view for the page
        PageLink link = stringToPageLink(newPage);
        PageDefaultSelection pageDefault = PageDefaults.value(link.ToObject);
        qDebug() << "PageDefault:" << newPage << pageDefault.Page << pageDefault.Function;
        if(!pageDefault.Page.isNull()) {
            bool okay = QMetaObject::invokeMethod(pageDefault.Page,
                                                  pageDefault.Function,
                                                  Q_ARG(QVariant, pageDefault.Parameters));
            if(!okay) {
                qDebug() << "Can't invokeMethod" << pageDefault.Page << link.Function << link.Parameters << LOCATION;
            }
        }
    }

    if(!LockHistory) {
        //Remove old pages
        int numberToRemove = (PageHistory.size() - 1) - CurrentPageIndex;
        for(int i = 0; i < numberToRemove; i++) {
            PageHistory.removeLast();
        }

        //Set the new current page
        PageHistory.append(newPage);
        CurrentPageIndex = PageHistory.size() - 1;
    }
}

/**
 * @brief cwPageSelectionModel::instertIntoPageTree
 * @param link
 *
 * This inserts the link into the page tree. Links can be inserted out of order. If inserted out of
 * order, ie link is register before parent pages, this will orphen the link and print a warning messsage.
 *
 * This function will then try to instert orphen links.
 */
void cwPageSelectionModel::insertIntoPageTree(const cwPageSelectionModel::PageLink &link, bool appendToOrphenLinks)
{
    PageTreeNodePtr parentNode;

    PageTreeNodePtr childNode(new PageTreeNode());
    childNode->Link = link;

    //The from object node already exists in the tree
    if(ObjectToPageLookup.contains(link.FromObject)) {
        //Add the link as a child of the fromObject
        parentNode = ObjectToPageLookup.value(link.FromObject);
    } else {
        //From object doesn't exist in the tree
        Q_ASSERT(!RootPage.isNull()); //Root page has been deleted or not set, via registerRootPage
        if(link.FromObject == RootPage) {
            //Add to RootNode
            parentNode = RootNode;
        } else {
            //This is an orphin link
            if(appendToOrphenLinks) {
                qDebug() << "Found and orphened link" << link.Name << "To:" << link.ToObject << "From:" << link.FromObject << LOCATION;
                OrphenLinks.insert(link, 0);
            }
            return;
        }
    }

    //Add the child to the parentNode
    OrphenLinks.remove(link);
    parentNode->ChildNodes.insert(link.Name, childNode);
    ObjectToPageLookup.insert(link.ToObject, childNode);

    foreach(const PageLink& link, OrphenLinks.keys()) {
        //Recursive call for trying to append orphen links,
        insertIntoPageTree(link, false);
    }
}

/**
 * @brief cwPageSelectionModel::stringToPageLink
 * @param pageLinkString
 * @return
 *
 * This will iterate the through the pageTree starting from the RootNode and find the link base
 * on the string. The string will be broken into it's parents using "/" as a seperator. It's part
 * is used to traverse the tree.
 *
 * If the link can't be found, then this returns an empty link
 */
cwPageSelectionModel::PageLink cwPageSelectionModel::stringToPageLink(const QString &pageLinkString) const
{
    QStringList parts = pageLinkString.split("/");
    PageTreeNodePtr current = RootNode;

    foreach(QString part, parts) {
        if(!current->ChildNodes.contains(part)) {
            return PageLink();
        }
        current = current->ChildNodes.value(part);
    }

    return current->Link;
}
