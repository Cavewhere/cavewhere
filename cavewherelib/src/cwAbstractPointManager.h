/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWABSRTACTPOINTMANAGER_H
#define CWABSRTACTPOINTMANAGER_H

//Qt includes
#include <QObject>
#include <QUrl>
#include <QQuickItem>
#include <QQmlEngine>

//Our includes
// #include "cwTransformItemUpdater.h"
#include "cwSelectionManager.h"

/**
 * @brief The cwAbsrtactPointManager class
 *
 * This is an abstarct base class that provides methods to create points or label in 3d area, using
 * a transformUpdate.  This class also support simple selection.
 *
 */
class cwAbstractPointManager : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AbstractPointManager)
    QML_UNCREATABLE("AbstractPointManager is a abstract class")

    Q_PROPERTY(int selectedItemIndex READ selectedItemIndex WRITE setSelectedItemIndex NOTIFY selectedItemIndexChanged)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager WRITE setSelectionManager NOTIFY selectionManagerChanged)

public:
    explicit cwAbstractPointManager(QQuickItem *parent = 0);
    
    void clearSelection();
    int selectedItemIndex() const;
    void setSelectedItemIndex(int selectedItemIndex);
    Q_INVOKABLE QQuickItem* selectedItem() const;

    QList<QQuickItem *> items() const;

    cwSelectionManager* selectionManager() const;
    void setSelectionManager(cwSelectionManager* selectionManager);

signals:
    // void transformUpdaterChanged();
    void selectedItemIndexChanged();
    void selectionManagerChanged();
    void pointParentItemChanged();

public slots:
    
protected:
    virtual QString qmlSource() const = 0;
    virtual void updateItemData(QQuickItem* item, int pointIndex) = 0;
    virtual void updateItemPosition(QQuickItem* item, int pointIndex) = 0;
    void resizeNumberOfItems(int numberOfStations);
    void updateAllItemData();

protected slots:
    void pointAdded();
    void pointRemoved(int index);
    void pointsInserted(int begin, int end);
    void pointsRemoved(int begin, int end);
    void updateItemsPositions(int begin, int end);

private slots:
    void updateSelection();

private:
    //The new points will have the parent of this
    QQuickItem* m_pointParentItem = nullptr;

    //All the point items
    QQmlComponent* m_itemComponent; //Allows you to create point items
    QList<QQuickItem*> m_items; //A list of point items

    cwSelectionManager* m_selectionManager; //!< The selection manager to select point items

    int m_selectedItemIndex; //!< The currently selected index

    void createComponent();
    QQuickItem *createItem();
    void removeItem(int index);

    void privateUpdateItemPosition(int index);
    void privateUpdateItemData(QQuickItem* item, int index);
};

Q_DECLARE_METATYPE(cwAbstractPointManager*)

/**
Gets selectedStationIndex
*/
inline int cwAbstractPointManager::selectedItemIndex() const {
    return m_selectedItemIndex;
}

/**
  \brief Clears the selection
  */
inline void cwAbstractPointManager::clearSelection() {
    setSelectedItemIndex(-1);
}

/**
Gets selectionManager
*/
inline cwSelectionManager* cwAbstractPointManager::selectionManager() const {
    return m_selectionManager;
}

#endif // CWABSRTACTPOINTMANAGER_H
