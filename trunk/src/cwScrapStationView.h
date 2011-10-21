#ifndef CWSCRAPSTATIONVIEW_H
#define CWSCRAPSTATIONVIEW_H

#include <QDeclarativeItem>
class cwTransformUpdater;
class cwScrap;
class cwImageItem;

/**
  This class manages a list of station items that visualize all the stations in a scrap.
  */
class cwScrapStationView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)


public:
    explicit cwScrapStationView(QDeclarativeItem *parent = 0);
    
    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

signals:
    void transformUpdaterChanged();
    void scrapChanged();

public slots:

private:
    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;

    cwScrap* Scrap; //!< The scrap this is class keeps track of

    //All the NoteStationItems
    QDeclarativeComponent* StationItemComponent;
    QList<QDeclarativeItem*> StationItems;

    void createStationComponent();
    void addNewStationItem();
    void updateStationItemData(int index);

private slots:
    void stationAdded();
    void stationRemoved(int stationIndex);
    void udpateStationPosition(int stationIndex);
    void updateAllStations();
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

#endif // CWSCRAPSTATIONVIEW_H
