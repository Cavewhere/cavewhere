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
    };

    class PageParameter {
    public:
        QPointer<QObject> Object;
        QByteArray Function;
    };

    bool LockHistory;
    QList<QString> PageHistory;
    QString CurrentPage; //!<
    int CurrentPageIndex;

    QHash<QObject*, PageDefaultSelection> PageDefaults; //Key: page, value is how the page will clear it's selection
    QHash<QObject*, PageLink> PageLinks; //Key: parent, Value: Page
    QHash<QObject*, PageParameter> PageParameters; //Key: object, Value: PageSelection function
    QHash<QString, PageLink> StringToPageLinks; //Holds the whole url to PageLink

    QStringList pageLinkDifferance(QString newPageLink, QString oldPageLink) const;
    QStringList objectToLinkStringList(QObject* object) const;
    QStringList expandLink(QString link) const;

    void printPageHistory() const;

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
