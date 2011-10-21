#ifndef CWNOTESTATIONVIEW_H
#define CWNOTESTATIONVIEW_H

//Qt includes
#include <QDeclarativeItem>

//Our includes
class cwScrap;
class cwTransformUpdater;
class cwScrapStationView;
#include "cwNoteStation.h"
#include "cwNote.h"

class cwNoteStationView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged)

public:
    explicit cwNoteStationView(QDeclarativeItem *parent = 0);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwNoteStation selectNoteStation();
    void selectNoteStation(int index);

    cwNote* note() const;
    void setNote(cwNote* note);

signals:
    void transformUpdaterChanged();
    void noteChanged();

public slots:

private:
    cwNote* Note; //!< Note that all the station will be display

    //The selected station, NULL if no stations are selected
    QDeclarativeItem* SelectedStation;

    //All the ScrapViewItems
    QList<cwScrapStationView*> ScrapStationViews;

    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;

    void updateAllScrapViewItems();
    void updateScrapViewItem(int index);

    cwScrapStationView* createNewStationView();

    void setSelectedStation(QDeclarativeItem* stationItem);

private slots:
    void addScrapItem();

};

Q_DECLARE_METATYPE(cwNoteStationView*)

/**
  Gets the transform updater
  */
inline cwTransformUpdater* cwNoteStationView::transformUpdater() const {
    return TransformUpdater;
}

/**
    Gets note that this station view will display all the note stations
*/
inline cwNote* cwNoteStationView::note() const {
    return Note;
}
#endif // CWNOTESTATIONVIEW_H
