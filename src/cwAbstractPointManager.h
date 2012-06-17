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
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)

public:
    explicit cwAbstractPointManager(QObject *parent = 0);
    
    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    void clearSelection();
    int selectedIndex() const;
    void setSelectedIndex(int selectedStationIndex);
    QQuickItem* selectedItem() const;

signals:
    void transformUpdaterChanged();
    void selectedIndexChanged();

public slots:
    
protected:
    virtual QUrl qmlSource() const = 0;
    virtual void updatePoint(QQuickItem* item, int pointIndex) = 0;

    void updateAllStations();

    void addPoint();
    void removePoint(int pointIndex);

protected slots:
    void pointAdded();
    void pointRemoved(int stationIndex);
    void udpateStationPosition(int stationIndex);
    void updateAllStations();
    void updateAllStationData();

private:
    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;
    bool TransformNodeDirty;

    //All the NoteStationItems
    QQmlComponent* PointComponent;
    QList<QQuickItem*> PointItems;

    int SelectedIndex; //!< The currently selected index

    void createComponent();
    QQuickItem *addItem();

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
inline int cwAbstractPointManager::selectedStationIndex() const {
    return SelectedStationIndex;
}


/**
  \brief Clears the selection
  */
inline void cwAbstractPointManager::clearSelection() {
    setSelectedStationIndex(-1);
}


#endif // CWABSRTACTPOINTMANAGER_H
