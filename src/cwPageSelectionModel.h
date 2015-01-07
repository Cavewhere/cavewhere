/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWPAGESELECTIONMODEL_H
#define CWPAGESELECTIONMODEL_H

#include <QObject>
#include <QVariantMap>
#include <QPointer>

class cwPageSelectionModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool hasForward READ hasForward NOTIFY hasForwardChanged)
    Q_PROPERTY(bool hasBackward READ hasBackward NOTIFY hasBackwardChanged)

public:
    explicit cwPageSelectionModel(QObject *parent = 0);

    Q_INVOKABLE void registerRootPage(QObject* page);
    Q_INVOKABLE void registerDefaultPageSelection(QObject* page,
                                                  QByteArray function,
                                                  QVariantMap defaultSelection = QVariantMap());

    Q_INVOKABLE void registerPageLink(QObject* from,
                                      QObject* to,
                                      QString toName,
                                      QByteArray function,
                                      QVariantMap pageLink = QVariantMap());

    Q_INVOKABLE void registerSelection(QObject* object,
                                      QByteArray function);

    void setCurrentSelection(QObject* object,
                             QVariantMap parameters);

    QString currentPage() const;
    void setCurrentPage(QString currentPage);
    Q_INVOKABLE void setCurrentPage(QObject* from, QString subPage);

    bool hasForward() const;
    bool hasBackward() const;

    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
//    Q_INVOKABLE void reload();

signals:
    void currentPageChanged();
    void hasForwardChanged();
    void hasBackwardChanged();

public slots:

private:
    class PageDefaultSelection {
    public:
        QPointer<QObject> Page;
        QByteArray Function;
        QVariantMap Parameters;
    };

    class PageLink {
    public:
        QPointer<QObject> FromObject;
        QPointer<QObject> ToObject;
        QString Name;
        QByteArray Function; //Used to call the parentObject invokable function
        QVariantMap Parameters; //Used to call the function

        bool operator<(const PageLink& other) const {
            if(FromObject.data() != other.FromObject.data()) {
                return FromObject.data() < other.FromObject.data();
            }else if(ToObject.data() != other.ToObject.data()) {
                return ToObject.data() < other.ToObject.data();
            }
            return Name < other.Name;
        }
    };

        class PageParameter {
    public:
        QPointer<QObject> Object;
        QByteArray Function;
    };

    class PageTreeNode {

    public:
        PageLink Link; //The link to the page
        QHash<QString, QSharedPointer<PageTreeNode> > ChildNodes;
    };

    typedef QSharedPointer<PageTreeNode> PageTreeNodePtr;

    bool LockHistory;
    QList<QString> PageHistory;
    QString CurrentPage; //!<
    int CurrentPageIndex;

    QHash<QObject*, PageDefaultSelection> PageDefaults; //Key: page, value is how the page will clear it's selection
//    QHash<QObject*, PageLink> PageLinks; //Key: parent, Value: Page
    QHash<QObject*, PageParameter> PageParameters; //Key: object, Value: PageSelection function
//    QHash<QString, PageLink> StringToPageLinks; //Holds the whole url to PageLink

    QPointer<QObject> RootPage; //Where all the PageLink should start registering from here
    PageTreeNodePtr RootNode;
    QHash<QObject*, PageTreeNodePtr> ObjectToPageLookup; //Key: From page, value: PageNode in the page tree;
    QMap<PageLink, char> OrphenLinks; //Nodes that we added out of order, value isn't used, always 0

    QStringList pageLinkDifferance(QString newPageLink, QString oldPageLink) const;
    QStringList objectToLinkStringList(QObject* object) const;
    QStringList expandLink(QString link) const;

    void printPageHistory() const;

    void loadPage(QString newPage, QString oldPage);

    void insertIntoPageTree(const PageLink& link, bool appendToOrphenLinks = true);
    PageLink stringToPageLink(const QString& pageLinkString) const;
};

/**
* @brief cwPageSelectionModel::currentPage
* @return
*/
inline QString cwPageSelectionModel::currentPage() const {
    return CurrentPage;
}

/**
* @brief cwPageSelectionModel::hasForward
* @return
*/
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

#endif // CWPAGESELECTIONMODEL_H
