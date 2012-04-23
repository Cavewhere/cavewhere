#ifndef CWSCRAPSTATIONVIEW_H
#define CWSCRAPSTATIONVIEW_H

//Qt includes
#include <QQuickItem>

//Our includes
class cwTransformUpdater;
class cwScrap;
class cwImageItem;
class cwScrapItem;
#include "cwNoteStation.h"

/**
  This class manages a list of station items that visualize all the stations in a scrap.
  */
class cwScrapStationView : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(int selectedStationIndex READ selectedStationIndex WRITE setSelectedStationIndex NOTIFY selectedStationIndexChanged)
    Q_PROPERTY(cwScrapItem* scrapItem READ scrapItem WRITE setScrapItem NOTIFY scrapItemChanged)

public:
    explicit cwScrapStationView(QQuickItem *parent = 0);
    ~cwScrapStationView();

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

    void clearSelection();
    int selectedStationIndex() const;
    void setSelectedStationIndex(int selectedStationIndex);
    QQuickItem* selectedStationItem() const;
    cwNoteStation selectedNoteStation() const;

    cwScrapItem* scrapItem() const;
    void setScrapItem(cwScrapItem* scrapItem);

signals:
    void transformUpdaterChanged();
    void scrapChanged();
    void selectedStationIndexChanged();
    void scrapItemChanged();

public slots:

private:

    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;

    cwScrap* Scrap; //!< The scrap this is class keeps track of

    //All the NoteStationItems
    QQmlComponent* StationItemComponent;
    QList<QQuickItem*> StationItems;

    //Shows the shot lines
    QQuickItem* ShotLinesHandler;
    QGraphicsPathItem* ShotLines;

    cwScrapItem* ScrapItem; //!< For selection and holding the scrap

    int SelectedStationIndex; //!< The currently selected station index

    void createStationComponent();
    void addNewStationItem();
    void updateStationItemData(int index);



private slots:
    void stationAdded();
    void stationRemoved(int stationIndex);
    void udpateStationPosition(int stationIndex);
    void updateAllStations();
    void updateAllStationData();
    void updateShotLines();
};

Q_DECLARE_METATYPE(cwScrapStationView*)

/**
  Gets the transform updater
  */
inline cwTransformUpdater* cwScrapStationView::transformUpdater() const {
    return TransformUpdater;
}

/**
    Gets scrap that this class renders all the stations of
*/
inline cwScrap* cwScrapStationView::scrap() const {
    return Scrap;
}

/**
Gets selectedStationIndex
*/
inline int cwScrapStationView::selectedStationIndex() const {
    return SelectedStationIndex;
}

/**
  Gets scrapItem
  */
inline cwScrapItem* cwScrapStationView::scrapItem() const {
    return ScrapItem;
}

/**
  \brief Clears the selection
  */
inline void cwScrapStationView::clearSelection() {
    setSelectedStationIndex(-1);
}

#endif // CWSCRAPSTATIONVIEW_H
