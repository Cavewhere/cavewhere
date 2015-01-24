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

/**
 * @brief The cwPageSelectionModel class
 *
 * This class manages page links, current page, and history of pages.
 */
class cwPageSelectionModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentPageAddress READ currentPageAddress WRITE setCurrentPageAddress NOTIFY currentPageAddressChanged)
    Q_PROPERTY(cwPage* currentPage READ currentPage NOTIFY currentPageChanged)

    Q_PROPERTY(bool hasForward READ hasForward NOTIFY hasForwardChanged)
    Q_PROPERTY(bool hasBackward READ hasBackward NOTIFY hasBackwardChanged)

public:
    explicit cwPageSelectionModel(QObject *parent = 0);

    Q_INVOKABLE cwPage* registerPage(cwPage *parentPage,
                                         QString pageName,
                                         QQmlComponent* pageComponent,
                                         QVariantMap pageProperties = QVariantMap());
    Q_INVOKABLE cwPage* registerSubPage(cwPage* parentPage,
                                        QString pageName,
                                        QVariantMap pageProperties = QVariantMap());

    Q_INVOKABLE void unregisterPage(cwPage* page);

    QString currentPageAddress() const;
    void setCurrentPageAddress(const QString &address);
    Q_INVOKABLE void gotoPage(cwPage* page);
    Q_INVOKABLE void gotoPageByName(cwPage *parentPage, QString subPage);

    cwPage* currentPage() const;

    bool hasForward() const;
    bool hasBackward() const;

    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();

    Q_INVOKABLE void clear();

signals:
    void currentPageAddressChanged();
    void currentPageChanged();
    void hasForwardChanged();
    void hasBackwardChanged();

public slots:

private:
    cwPage* RootPage;

    bool LockHistory;
    QList<QPointer<cwPage> > PageHistory;
    QPointer<cwPage> CurrentPage; //!<
    int CurrentPageIndex;

    //This gets the unknown link address for the model
    //If the user enters a bad link, the text will be stored here
    QString UnknownLinkAddress;

    QStringList expandLink(QString link) const;

    void printPageHistory() const;

    cwPage* stringToPage(const QString& pageLinkString) const;

    bool isPageInModel(cwPage* page) const;

};


/**
 * @brief cwPageSelectionModel::hasForward
 * @return true if user can go forward in the page history and false if forward() will
 * do nothing.
 */
inline bool cwPageSelectionModel::hasForward() const {
    return CurrentPageIndex < PageHistory.size() - 1;
}

/**
* @brief cwPageSelectionModel::hasBackward
* @return true if the user can go backward in the page history and false if back() will
* do nothing
*/
inline bool cwPageSelectionModel::hasBackward() const {
    return CurrentPageIndex > 0;
}

#endif // CWPAGESELECTIONMODEL_H
