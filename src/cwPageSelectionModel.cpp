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
    CurrentPageIndex(0)
{

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

    PageLinks.insert(to, link);
    StringToPageLinks.insert(objectToLinkStringList(to).join("/"), link);
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
* @param currentPage
*/
void cwPageSelectionModel::setCurrentPage(QString currentPage) {
    if(currentPage.isEmpty()) {
        return;
    }

    if(CurrentPage != currentPage) {

        QStringList newPageList = pageLinkDifferance(currentPage, CurrentPage);
        foreach(QString page, newPageList) {
            //Go to the page
            PageLink link = StringToPageLinks.value(page);
            if(!link.FromObject.isNull()) {
                bool okay = QMetaObject::invokeMethod(link.FromObject,
                                                      link.Function,
                                                      Q_ARG(QVariant, link.Parameters));
                if(!okay) {
                    qDebug() << "Can't invokeMethod" << link.FromObject << link.Function << link.Parameters << LOCATION;
                }
            } else {
                qDebug() << "Link parent object is null, this is a bug" << LOCATION;
            }
        }

        if(!newPageList.isEmpty()) {
            //Load the default view for the page
            PageLink link = StringToPageLinks.value(currentPage);
            PageDefaultSelection pageDefault = PageDefaults.value(link.ToObject);
            qDebug() << "PageDefault:" << currentPage << pageDefault.Page << pageDefault.Function;
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
            PageHistory.append(currentPage);
            CurrentPageIndex = PageHistory.size() - 1;
        }

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
    QStringList pageNames;

    PageLink link = PageLinks.value(object);
    while(!link.Name.isEmpty()) {
        pageNames.prepend(link.Name);

        //Go up to the parent link
        object = link.FromObject;
        link = PageLinks.value(object);
    }

    return pageNames;
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
