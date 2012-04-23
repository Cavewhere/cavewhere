#ifndef CWSCRAPITEM_H
#define CWSCRAPITEM_H

//Qt includes
#include <QQuickItem>

//Our includes
class cwScrap;
class cwTransformUpdater;
class cwScrapStationView;

/**
  \brief This draws a scrap
  */
class cwScrapItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrapStationView* stationView READ stationView NOTIFY stationViewChanged)

public:
    explicit cwScrapItem(QQuickItem *parent = 0);
    explicit cwScrapItem(QDeclarativeContext* context, QQuickItem *parent = 0);
    ~cwScrapItem();

    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

    bool isSelected() const;
    void setSelected(bool selected);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* transformUpdater);

    cwScrapStationView* stationView() const;

signals:
    void scrapChanged();
    void selectedChanged();
    void transformUpdaterChanged();
    void stationViewChanged();

public slots:

private:
    //Data class
    cwScrap* Scrap;

    //For keeping the 2D object aligned
    cwTransformUpdater* TransformUpdater; //!<

    //Visual elements
    QQuickItem* BorderItemHandler;
    //FIXME: Implement painting method for border item
//    QGraphicsPolygonItem* BorderItem; //!< The border around the scrap
    cwScrapStationView* StationView; //!< All the stations in the scrap

    bool Selected; //!< True if the scrap is select and false if it isn't

private slots:
    void updateScrapGeometry();

};

Q_DECLARE_METATYPE(cwScrapItem*)

/**
  Gets the scrap this scrap will scrap item will draw
  */
inline cwScrap* cwScrapItem::scrap() const {
    return Scrap;
}


/**
  Returns true if the scrap item is select and false if it isn't
  */
inline bool cwScrapItem::isSelected() const {
    return Selected;
}

/**
Gets transformUpdater
*/
inline cwTransformUpdater* cwScrapItem::transformUpdater() const {
    return TransformUpdater;
}

/**
Gets stationView, this stores all the note station item
*/
inline cwScrapStationView* cwScrapItem::stationView() const {
    return StationView;
}

#endif // CWSCRAPITEM_H
