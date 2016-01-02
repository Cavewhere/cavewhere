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
}

/**
 * @brief cwPageSelectionModel::registerPage
 * @param parentPageItem - The parent page of the registeredPage
 * @param pageName - The name of the page
 * @param pageComponent - The component that will be load dynamically when the page is loaded
 * @param pageProperties - The properties of the instantiated component
 * @return cwPage that was registered in the model
 *
 * The page name can be change ofter the page has been registered by calling cwPage::setName. This
 * will update the link automatically. All other parameters of the cwPage are immutable. If you want
 * to change the paramaters of a page, you must unregister the page by calling unregisterPage(),
 * and recreate the page by calling registerPage again.
 *
 * The page's are owned by the cwPageSelectionModel and shouldn't be delete. If you want to delete
 * a page, call unregisterPage()
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

    Q_ASSERT(isPageInModel(parentPage)); //Make sure the parent page exists in the model

    cwPage* newPage = nullptr;

    //Try to update the history, when the page with the same name is re-registered
    cwPage* childPage = parentPage->childPage(pageName);
    if(childPage != nullptr) {
        parentPage->removeChild(childPage);
        childPage->deleteLater();
        newPage = new cwPage(parentPage, pageComponent, pageName, pageProperties);

        for(int i = 0; i < PageHistory.size(); i++) {
            cwPage* pastPage = PageHistory.at(i);
            if(pastPage == childPage) {
                PageHistory.replace(i, newPage);
            }
        }

        emit historyChanged();

        if(CurrentPage == childPage) {
            //Replace the current page with the new page
            CurrentPage = newPage;
        }
    }

    if(newPage == nullptr) {
        cwPage* page = new cwPage(parentPage, pageComponent, pageName, pageProperties);
        return page;
    }
    return newPage;
}

/**
 * @brief cwPageSelectionModel::registerSubPage
 * @param parentPage
 * @param pageName
 * @param pageProperties
 * @return The page that was registered in the mode
 *
 * This function is slightly different the registerPage(). This is useful for registering a
 * different view inside of the current page. Basicially, both the parentPage and the page that is
 * returned from this function has the same QQmlComponent and the pageProperties are different.
 *
 * For example, this is used with the Carpet Mode in the SurveyEditor
 */
cwPage *cwPageSelectionModel::registerSubPage(cwPage *parentPage, QString pageName, QVariantMap pageProperties)
{
    Q_ASSERT(parentPage != nullptr);

    //Inherite properties from the parent
    QVariantMap properties = parentPage->pageProperties();
    foreach(QString key, pageProperties.keys()) {
        properties.insert(key, pageProperties.value(key));
    }

    return registerPage(parentPage, pageName, parentPage->component(), properties);
}

/**
 * @brief cwPageSelectionModel::unregisterPage
 * @param page - The page that will be unregistered
 *
 * The page must be registered with this page selection model. If it's not this function will
 * assert!
 *
 * The page is deleted after calling this function and shouldn't be used.
 */
void cwPageSelectionModel::unregisterPage(cwPage *page)
{
    if(page != nullptr) {
        Q_ASSERT(page != currentPage());

        if(!PageHistory.contains(page)) {
            //Delay removing the page, because it's in the history.
            page->parentPage()->removeChild(page);
            page->deleteLater();
        }
    } else {
        qDebug() << "Unregistering a null page! This is a bug" << LOCATION;
    }
}

/**
* @brief cwPageSelectionModel::setCurrentPageAddress
* @param address - The string address for a page
*
* This will attempt lookup the page with the address. If the page that is found is valid,
* it will set page to be the current page. If the address points to a unknown location
* the current page will be set to null. This will probably cause the cwPageView to display an
* unknown page to the user.
*/
void cwPageSelectionModel::setCurrentPageAddress(const QString& address) {
    if(address.isEmpty()) {
        return;
    }

    if(currentPageAddress() != address) {
        cwPage* page = stringToPage(address);

        if(page != nullptr) {
            gotoPage(page);
        } else {
            //Don't know how to go to link, perhaps, it's not register yet
            //This will go through all the parent pages until we get to the full
            //link address
            QStringList expandedLinks = expandLink(address);
            for(int i = 0; i < expandedLinks.size(); i++) {
                QString parentLink = expandedLinks.at(i);
                page = stringToPage(parentLink);

                if(page == nullptr) {
                    UnknownLinkAddress = address;
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
 * @param page - Sets the current page
 *
 * The page can be null. If the page is null, this will probably cause the cwPageView to display
 * an unknown page to the user.
 *
 * If the page doesn't exist in the model, this function will assert.
 */
void cwPageSelectionModel::gotoPage(cwPage *page)
{
    Q_ASSERT(isPageInModel(page));

    if(CurrentPage != page) {

        if(CurrentPage != nullptr) {
            disconnect(CurrentPage.data(), &cwPage::nameChanged, this, &cwPageSelectionModel::currentPageAddressChanged);
        }

        CurrentPage = page;

        if(CurrentPage != nullptr) {
            connect(CurrentPage.data(), &cwPage::nameChanged, this, &cwPageSelectionModel::currentPageAddressChanged);
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

            emit historyChanged();

        }

//        printPageHistory();

        emit currentPageChanged();
        emit currentPageAddressChanged();
        emit hasBackwardChanged();
        emit hasForwardChanged();
    }
}

/**
 * @brief cwPageSelectionModel::gotoPageByName
 * @param parentPage - The parent page
 * @param pageLink - The subpage name
 *
 * This is a convenience function for gotoPage(). This allows for parentPage and a subpage name
 * to be used to goto a particualar page. This is useful for qml.
 */
void cwPageSelectionModel::gotoPageByName(cwPage *parentPage, QString subPage)
{
    if(parentPage == nullptr) {
        parentPage = RootPage;
    }

    cwPage* childPage = parentPage->childPage(subPage);

    if(childPage == nullptr) {
        UnknownLinkAddress = QString("%1%2%3").arg(parentPage->fullname()).arg(seperator()).arg(subPage);
    }

    gotoPage(childPage);
}

/**
 * @brief cwPageSelectionModel::back
 *
 * This will cause the page selection model to go back a single page. If there isn't any more
 * pages to go back ie. hasBackward() returns false, this does nothing.
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
 *
 * This will cause the page selection model to go forward a single page. If there isn't any more
 * pages to go foward ie. hasFoward() returns false, this does nothing.
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
 * @brief cwPageSelectionModel::clear
 *
 * This clears the selection model. Delete's the root page and all of the sub children,
 * clears the history, and makes the current page null.
 */
void cwPageSelectionModel::clear()
{
    RootPage->deleteLater();
    RootPage = new cwPage();
    RootPage->setParent(this);
    CurrentPage.clear();
    PageHistory.clear();

    emit currentPageAddressChanged();
    emit currentPageChanged();
}

/**
 * @brief cwPageSelectionModel::seperator
 * @return "/"
 */
QString cwPageSelectionModel::seperator()
{
    return QString("/");
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
    QStringList linkParts = link.split(seperator());

    QStringList expandedLinks;
    expandedLinks.reserve(linkParts.size());

    //Populate the expandedLinks with empty strings
    for(int i = 0; i < linkParts.size(); i++) {
        expandedLinks.append(QString());
    }

    for(int i = 0; i < linkParts.size(); i++) {
        QString linkPart = linkParts.at(i);

        if(i != 0) {
            linkPart = seperator() + linkPart;
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
 * @brief cwPageSelectionModel::stringToPageLink
 * @param pageLinkString
 * @return
 *
 * This will iterate the through the pageTree starting from the RootNode and find the link base
 * on the string. The string will be broken into it's parents using seperator(). It's part
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
 * @return True if the any of the parents are RootPage. False if page isn't in the selection model
 */
bool cwPageSelectionModel::isPageInModel(cwPage *page) const
{
    if(page == nullptr) {
        return true;
    }

    while(page != RootPage && page != nullptr) {
        page = page->parentPage();
    }
    return page == RootPage;
}

/**
* @brief cwPageSelectionModel::currentPage
* @return Retruns the full name of the current page
*
* You can use this to call setCurrentLink()
*/
QString cwPageSelectionModel::currentPageAddress() const {
    if(CurrentPage != nullptr) {
        return CurrentPage->fullname();
    }
    return UnknownLinkAddress;
}


/**
* @brief cwPageSelectionModel::hasForward
* @return Returns the current page that the page selection model is pointing to
*/
cwPage *cwPageSelectionModel::currentPage() const
{
    return CurrentPage;
}

/**
 * @brief cwPageSelectionModel::clearHistory
 *
 * This will clear the history for the page selection model
 */
void cwPageSelectionModel::clearHistory()
{
    CurrentPage.clear();
    PageHistory.clear();
}

/**
* @brief cwPageSelectionModel::history
* @return The page history
*/
QList<QObject *> cwPageSelectionModel::history() const {
    QList<QObject*> pages;
    foreach(cwPage* page, PageHistory) {
        pages.append(page);
    }
    return pages;
}

