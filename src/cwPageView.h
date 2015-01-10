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
class cwPage;
#include "cwPageViewAttachedType.h"

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
    Q_PROPERTY(QQmlComponent* unknownPageComponent READ unknownPageComponent WRITE setUnknownPageComponent NOTIFY unknownPageComponentChanged)

public:
    cwPageView(QQuickItem* parentItem = nullptr);
    ~cwPageView();

    cwPageSelectionModel* pageSelectionModel() const;
    void setPageSelectionModel(cwPageSelectionModel* pageSelectionModel);

    QQmlComponent* unknownPageComponent() const;
    void setUnknownPageComponent(QQmlComponent* unknownPageComponent);

    /**
     * @brief cwPageView::qmlAttachedProperties
     * @param object
     * @return Return a attachment object for object
     *
     * This attaches pageViewAttachedType to object
     */
    static cwPageViewAttachedType* qmlAttachedProperties(QObject* object) {
        qDebug() << "Attaching page view object to" << object;
        return new cwPageViewAttachedType(object);
    }

signals:
    void pageSelectionModelChanged();
    void unknownPageComponentChanged();

public slots:

private slots:
    void loadCurrentPage();

private:
    QPointer<cwPageSelectionModel> PageSelectionModel; //!<
    QHash<QQmlComponent*, QQuickItem*> ComponentToItem;
    QList<QQuickItem*> PageItems;

    //For displaying the unknown page
    QPointer<QQmlComponent> UnknownPageComponent; //!<
    QQuickItem* UnknownPageItem;

    void updateProperties(QQuickItem* pageItem, cwPage* page);
    void showPage(QQuickItem* pageItem);
    QQuickItem* createChildItemFromComponent(QQmlComponent* component, cwPage* page);

};

QML_DECLARE_TYPEINFO(cwPageView, QML_HAS_ATTACHED_PROPERTIES)



#endif // CWPAGEVIEW_H
