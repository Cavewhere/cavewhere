#ifndef CWSCRAPSTATIONVIEW_H
#define CWSCRAPSTATIONVIEW_H

//Qt includes
#include <QQuickItem>

//Our includes
class cwTransformUpdater;
class cwScrap;
class cwImageItem;
class cwScrapItem;
class cwSGLinesNode;
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
    Q_PROPERTY(float shotLineScale READ shotLineScale WRITE setShotLineScale NOTIFY shotLineScaleChanged)

public:
    explicit cwScrapStationView(QQuickItem *parent = 0);
    ~cwScrapStationView();

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

    float shotLineScale() const;
    void setShotLineScale(float scale);

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
    void shotLineScaleChanged();

public slots:

private:

    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;
    bool TransformNodeDirty;

    cwScrap* Scrap; //!< The scrap this is class keeps track of

    //All the NoteStationItems
    QQmlComponent* StationItemComponent;
    QList<QQuickItem*> StationItems;

    //Geometry for the shot lines
    cwSGLinesNode* ShotLinesNode;
    QVector<QLineF> ShotLines;

    float ShotLineScale;

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

protected:
    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);

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

/**
 * @brief cwScrapStationView::shotLineScale
 * @return Return's the shot line scale, 0.0 to 1.0, useful for animating the shot lines
 */
inline float cwScrapStationView::shotLineScale() const {
    return ShotLineScale;
}


#endif // CWSCRAPSTATIONVIEW_H
