#ifndef CWNOTESTATIONVIEW_H
#define CWNOTESTATIONVIEW_H

//Qt includes
#include <QDeclarativeItem>

//Our includes
class cwScrap;
class cwTransformUpdater;
#include "cwNoteStation.h"
#include "cwNote.h"

class cwNoteStationView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)


public:
    explicit cwNoteStationView(QDeclarativeItem *parent = 0);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwScrap* scrap() const;
    void setScrap(cwScrap* scrap);

    cwNoteStation selectNoteStation();
    void selectNoteStation(int index);

signals:
    void scrapChanged();
    void transformUpdaterChanged();

public slots:

private:
    //The scrap where all the stations are located
    cwScrap* Scrap;

    //The selected station, NULL if no stations are selected
    QDeclarativeItem* SelectedStation;

    //All the NoteStationItems
    QDeclarativeComponent* NoteStationComponent;
    QList<QDeclarativeItem*> NoteStationItems;

    //Will keep the note stations at the correct location
    cwTransformUpdater* TransformUpdater;

    void updateAllNoteStationItems();
    void updateNote();

    void setSelectedStation(QDeclarativeItem* stationItem);
private slots:
    void addNoteStationItem();

};

Q_DECLARE_METATYPE(cwNoteStationView*)

/**
  Gets the scrap that this note station view will add stations to
  */
inline cwScrap* cwNoteStationView::scrap() const {
    return Scrap;
}

/**
  Gets the transform updater
  */
inline cwTransformUpdater* cwNoteStationView::transformUpdater() const {
    return TransformUpdater;
}

#endif // CWNOTESTATIONVIEW_H
