/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWPAGESELECTIONMODEL_H
#define CWPAGESELECTIONMODEL_H

//Qt includes
#include <QObject>
#include <QVariantMap>
#include <QPointer>
#include <QQmlComponent>
#include <QUuid>
#include <QQuickItem>

//Our includes
class cwPage;

class cwPageSelectionModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentLink READ currentLink WRITE setCurrentLink NOTIFY currentLinkChanged)
    Q_PROPERTY(cwPage* currentPage READ currentPage NOTIFY currentPageChanged)

    Q_PROPERTY(bool hasForward READ hasForward NOTIFY hasForwardChanged)
    Q_PROPERTY(bool hasBackward READ hasBackward NOTIFY hasBackwardChanged)

public:
    explicit cwPageSelectionModel(QObject *parent = 0);


//    Q_INVOKABLE void registerRootPage(QObject* page);
//    Q_INVOKABLE void registerDefaultPageSelection(QObject* page,
//                                                  QByteArray function,
//                                                  QVariantMap defaultSelection = QVariantMap());

    void registerQuickItemToPage(QQuickItem* pageItem, cwPage* page);
    Q_INVOKABLE cwPage* registerPage(QQuickItem *parentPageItem,
                                         QString pageName,
                                         QQmlComponent* pageComponent,
                                         QVariantMap pageProperties = QVariantMap());

//    Q_INVOKABLE void registerSelection(QObject* object,
//                                      QByteArray function);

//    void setCurrentSelection(QObject* object,
//                             QVariantMap parameters);



    QString currentLink() const;
    void setCurrentLink(QString link);
    Q_INVOKABLE void gotoPage(cwPage* page);
    Q_INVOKABLE void gotoPageByName(QQuickItem* parentPage, QString subPage);

    cwPage* currentPage() const;

    bool hasForward() const;
    bool hasBackward() const;

    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
//    Q_INVOKABLE void reload();

signals:
    void currentLinkChanged();
    void currentPageChanged();
    void hasForwardChanged();
    void hasBackwardChanged();
//    void currentPageComponentChanged();

public slots:

private:
    class DelayedPage {
    public:
        DelayedPage() {}
        DelayedPage(QQuickItem *parentPageItem,
                    QString pageName,
                    QQmlComponent *pageComponent,
                    QVariantMap pageProperties) :
            ParentPageItem(parentPageItem),
            PageName(pageName),
            PageComponent(pageComponent),
            PageProperties(pageProperties) {}

        QQuickItem *ParentPageItem;
        QString PageName;
        QQmlComponent *PageComponent;
        QVariantMap PageProperties;
    };

//    class PageDefaultSelection {
//    public:
//        QPointer<QObject> Page;
//        QByteArray Function;
//        QVariantMap Parameters;
//    };



//        class PageParameter {
//    public:
//        QPointer<QObject> Object;
//        QByteArray Function;
//    };

//    class PageTreeNode {

//    public:
//        PageLink Link; //The link to the page
//        QHash<QString, QSharedPointer<PageTreeNode> > ChildNodes;
//    };

//    typedef QSharedPointer<PageTreeNode> PageTreeNodePtr;

    cwPage* RootPage;

    bool LockHistory;
    QList<cwPage*> PageHistory;
    cwPage* CurrentPage; //!<
    int CurrentPageIndex;
//    QPointer<QQmlComponent> CurrentPageComponent; //!<

    //This allow's us to lookup the parent page, when calling registerPage
    QHash<QQuickItem*, cwPage*> QuickItemToPage;

    QHash<QQuickItem*, DelayedPage> DelayRegisterationPages;


//    QHash<QObject*, PageDefaultSelection> PageDefaults; //Key: page, value is how the page will clear it's selection
//    QHash<QObject*, PageLink> PageLinks; //Key: parent, Value: Page
//    QHash<QObject*, PageParameter> PageParameters; //Key: object, Value: PageSelection function
//    QHash<QString, PageLink> StringToPageLinks; //Holds the whole url to PageLink

//    QPointer<QObject> RootPage; //Where all the PageLink should start registering from here
//    PageTreeNodePtr RootNode;
//    QHash<QObject*, PageTreeNodePtr> ObjectToPageLookup; //Key: From page, value: PageNode in the page tree;
//    QMap<PageLink, char> OrphenLinks; //Nodes that we added out of order, value isn't used, always 0

//    QStringList pageLinkDifferance(QString newPageLink, QString oldPageLink) const;
//    QStringList objectToLinkStringList(QObject* object) const;
//    QStringList expandLink(QString link) const;

    void printPageHistory() const;

//    void loadPage(QString newPage, QString oldPage);

//    void insertIntoPageTree(const PageLink& link, bool appendToOrphenLinks = true);
    cwPage* stringToPage(const QString& pageLinkString) const;
};

//Q_DECLARE_METATYPE(cwPageSelectionModel::PagePtr)



/**
* @brief cwPageSelectionModel::hasForward
* @return Returns the current page that the page selection model is pointing to
*/
inline cwPage *cwPageSelectionModel::currentPage() const
{
    return CurrentPage;
}

inline bool cwPageSelectionModel::hasForward() const {
    return CurrentPageIndex < PageHistory.size() - 1;
}

/**
* @brief cwPageSelectionModel::hasBackward
* @return
*/
inline bool cwPageSelectionModel::hasBackward() const {
    return CurrentPageIndex > 0;
}


///**
//* @brief cwPageSelectionModel::currentPageComponent
//* @return
//*/
//inline QQmlComponent* cwPageSelectionModel::currentPageComponent() const {
//    return CurrentPageComponent;
//}

#endif // CWPAGESELECTIONMODEL_H
