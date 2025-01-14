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
#include <QQmlEngine>

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
    QML_NAMED_ELEMENT(PageView)
    QML_ATTACHED(cwPageViewAttachedType)

    Q_PROPERTY(cwPageSelectionModel* pageSelectionModel READ pageSelectionModel WRITE setPageSelectionModel NOTIFY pageSelectionModelChanged)
    Q_PROPERTY(QQmlComponent* unknownPageComponent READ unknownPageComponent WRITE setUnknownPageComponent NOTIFY unknownPageComponentChanged)
    Q_PROPERTY(QQuickItem* currentPageItem READ currentPageItem NOTIFY currentPageItemChanged)

public:
    cwPageView(QQuickItem* parentItem = nullptr);
    ~cwPageView();

    cwPageSelectionModel* pageSelectionModel() const;
    void setPageSelectionModel(cwPageSelectionModel* pageSelectionModel);

    QQmlComponent* unknownPageComponent() const;
    void setUnknownPageComponent(QQmlComponent* unknownPageComponent);

    QQuickItem* currentPageItem() const;

    Q_INVOKABLE QQuickItem* pageItem(cwPage* page);

    /**
     * @brief cwPageView::qmlAttachedProperties
     * @param object
     * @return Return a attachment object for object
     *
     * This attaches pageViewAttachedType to object. This function should never be called
     * expected from the Qt Framework
     */
    static cwPageViewAttachedType* qmlAttachedProperties(QObject* object) {
        return new cwPageViewAttachedType(object);
    }

signals:
    void pageSelectionModelChanged();
    void unknownPageComponentChanged();
    void currentPageItemChanged();

public slots:

private slots:
    void loadCurrentPage();
    void updateSelectionOnCurrentPage();

private:
    QPointer<cwPageSelectionModel> PageSelectionModel; //!<
    QPointer<cwPage> CurrentPage; //The current page that's begin viewed
    QHash<QQmlComponent*, QQuickItem*> ComponentToItem;
    QList<QQuickItem*> PageItems;

    //For displaying the unknown page
    QPointer<QQmlComponent> UnknownPageComponent; //!<
    QQuickItem* UnknownPageItem;

    void updateProperties(QQuickItem* pageItem, cwPage* page);
    void updateProperties(QQuickItem* pageItem, QVariantMap defaultProperties, QVariantMap properties);

    void showPage(QQuickItem* pageItem);
    QQuickItem* createChildItemFromComponent(QQmlComponent* component, cwPage* page);

};

QML_DECLARE_TYPEINFO(cwPageView, QML_HAS_ATTACHED_PROPERTIES)



#endif // CWPAGEVIEW_H
