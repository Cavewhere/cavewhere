/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWPAGEVIEW_H
#define CWPAGEVIEW_H

//Qt includes
#include <QQuickItem>
#include <QPointer>

//Our includes
class cwPageSelectionModel;

/**
 * @brief The cwPageView class
 *
 * This manages and creates the pages from the page selection model. This class preforms lazy
 * instantaction of QQuickItem from the currentPage in the pageSelectionModel. This class will
 * also manage how the pages are display (showing and hiding them).
 */
class cwPageView : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(cwPageSelectionModel* pageSelectionModel READ pageSelectionModel WRITE setPageSelectionModel NOTIFY pageSelectionModelChanged)

public:
    cwPageView(QQuickItem* parentItem = nullptr);
    ~cwPageView();

    cwPageSelectionModel* pageSelectionModel() const;
    void setPageSelectionModel(cwPageSelectionModel* pageSelectionModel);

signals:
    void pageSelectionModelChanged();

public slots:

private slots:
    void loadCurrentPage();

private:
    QPointer<cwPageSelectionModel> PageSelectionModel; //!<
    QHash<QQmlComponent*, QQuickItem*> ComponentToItem;
    QList<QQuickItem*> PageItems;

    void updateProperties(QQuickItem* pageItem, QVariantMap properties);
    void showPage(QQuickItem* pageItem);

};



#endif // CWPAGEVIEW_H
