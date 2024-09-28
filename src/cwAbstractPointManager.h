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

//Our includes
#include "cwTransformUpdater.h"
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

    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(int selectedItemIndex READ selectedItemIndex WRITE setSelectedItemIndex NOTIFY selectedItemIndexChanged)
    Q_PROPERTY(cwSelectionManager* selectionManager READ selectionManager WRITE setSelectionManager NOTIFY selectionManagerChanged)

public:
    explicit cwAbstractPointManager(QQuickItem *parent = 0);
    
    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    void clearSelection();
    int selectedItemIndex() const;
    void setSelectedItemIndex(int selectedItemIndex);
    Q_INVOKABLE QQuickItem* selectedItem() const;

    QList<QQuickItem*> items() const;

    cwSelectionManager* selectionManager() const;
    void setSelectionManager(cwSelectionManager* selectionManager);

signals:
    void transformUpdaterChanged();
    void selectedItemIndexChanged();
    void selectionManagerChanged();

public slots:
    
protected:
    virtual QUrl qmlSource() const = 0;
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
    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;

    //All the point items
    QQmlComponent* ItemComponent; //Allows you to create point items
    QList<QQuickItem*> Items; //A list of point items

    cwSelectionManager* SelectionManager; //!< The selection manager to select point items

    int SelectedItemIndex; //!< The currently selected index

    void createComponent();
    QQuickItem* createItem();
    void removeItem(int index);

    void privateUpdateItemPosition(int index);
    void privateUpdateItemData(QQuickItem* item, int index);
};

Q_DECLARE_METATYPE(cwAbstractPointManager*)

/**
  Gets the transform updater
  */
inline cwTransformUpdater* cwAbstractPointManager::transformUpdater() const {
    return TransformUpdater;
}

/**
Gets selectedStationIndex
*/
inline int cwAbstractPointManager::selectedItemIndex() const {
    return SelectedItemIndex;
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
    return SelectionManager;
}

#endif // CWABSRTACTPOINTMANAGER_H
