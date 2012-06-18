#ifndef CWABSRTACTPOINTMANAGER_H
#define CWABSRTACTPOINTMANAGER_H

//Qt includes
#include <QObject>
#include <QUrl>
#include <QQuickItem>

//Our includes
class cwTransformUpdater;

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

public:
    explicit cwAbstractPointManager(QQuickItem *parent = 0);
    
    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    void clearSelection();
    int selectedItemIndex() const;
    void setSelectedItemIndex(int selectedItemIndex);
    QQuickItem* selectedItem() const;

signals:
    void transformUpdaterChanged();
    void selectedItemIndexChanged();

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
    void updateItemPosition(int index);


private:
    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;

    //All the NoteStationItems
    QQmlComponent* ItemComponent;
    QList<QQuickItem*> Items;

    int SelectedItemIndex; //!< The currently selected index

    void createComponent();
    QQuickItem* addItem();
    void removeItem(int index);
};

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


#endif // CWABSRTACTPOINTMANAGER_H
