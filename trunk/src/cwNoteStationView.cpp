//Our includes
#include "cwNoteStationView.h"
#include "cwScrap.h"
#include "cwTransformUpdater.h"
#include "cwDebug.h"
#include "cwNote.h"
#include "cwScrapStationView.h"

//Qt includes
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QDebug>

cwNoteStationView::cwNoteStationView(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{

}

///**
//  Sets the scrap for the station to be generated
//  */
//void cwNoteStationView::setScrap(cwScrap* scrap) {
//    if(Scrap != scrap) {
//        Scrap = scrap;
//        emit scrapChanged();
//    }
//}

/**
  \brief Sets the selected station.

  This will deselect the previous station.  Send this null to deselect the current
  station
  */
void cwNoteStationView::setSelectedStation(QDeclarativeItem* selectedStation) {
    if(SelectedStation != NULL && SelectedStation->property("selected").toBool() != false) {
        SelectedStation->setProperty("selected", false);
    }

    SelectedStation = selectedStation;

    if(SelectedStation != NULL && SelectedStation->property("selected").toBool() != true) {
        SelectedStation->setProperty("selected", true);
    }
}

/**
  \brief Addes a new scrap item to the station view

  This is classed when the note adds a scrap
  */
void cwNoteStationView::addScrapItem() {
    if(Note == NULL) { return; }

    //Create a new ScrapStationView
    cwScrapStationView* stationView = createNewStationView();
    ScrapStationViews.append(stationView);

    //Update the stationView item
    int lastScrapStationViewIndex = ScrapStationViews.size() - 1;
    updateScrapViewItem(lastScrapStationViewIndex);

    Q_ASSERT(ScrapStationViews.size() == Note->scraps().size());
}

/**
  \brief Sets the transform updater

  This will transform all the station in this station view
  */
void cwNoteStationView::setTransformUpdater(cwTransformUpdater* updater) {
    if(TransformUpdater != updater) {

        TransformUpdater = updater;

        foreach(cwScrapStationView* scrapStationView, ScrapStationViews) {
            scrapStationView->setTransformUpdater(TransformUpdater);
        }

        emit transformUpdaterChanged();
    }
}

/**
  This gets the select note station from the view

  If no station is select, this returns an empty note station
  */
cwNoteStation cwNoteStationView::selectNoteStation() {
//    cwNoteStation selectedStation;
//    if(SelectedStation != NULL &&
//            Scrap != NULL) {
//        int stationId = SelectedStation->property("stationId").toInt();
//        selectedStation = Scrap->station(stationId);
//    }
//    return selectedStation;
    return cwNoteStation();
}

/**
  Selects the station at index, if index is out of bounds, this does nothing
  */
void cwNoteStationView::selectNoteStation(int index) {
//    if(index < 0 || index >= NoteStationItems.size()) {
//        qDebug() << "Can't select index, out of range" << index << LOCATION;
//        return;
//    }

//    NoteStationItems[index]->setProperty("selected", true);
}

/**
  Sets note for the station view to visualize.  All the station in the page of
  notes will be visualized.
*/
void cwNoteStationView::setNote(cwNote* note) {
    if(Note != note) {

        if(Note != NULL) {
            disconnect(Note, NULL, this, NULL);
        }

        Note = note;

        if(Note != NULL) {
            updateAllScrapViewItems();

            connect(Note, SIGNAL(scrapAdded()), SLOT(addScrapItem()));
        }

        emit noteChanged();
    }
}

/**
  Updates all the scrap view items

  Based on the current note, this will try to re-use the view items.

  Extra view items will be deleted
  */
void cwNoteStationView::updateAllScrapViewItems() {
    if(Note == NULL) {
        return;
    }

    if(ScrapStationViews.size() < Note->scraps().size()) {
        int scrapsToAdd = Note->scraps().size() - ScrapStationViews.size();
        //Add more station views to
        for(int i = 0; i < scrapsToAdd; i++) {
            int noteStationIndex = ScrapStationViews.size();

            if(noteStationIndex < 0 || noteStationIndex >= Note->scraps().size()) {
                qDebug() << "noteStationIndex out of bounds:" << noteStationIndex << ScrapStationViews.size();
                continue;
            }

            cwScrapStationView* stationView = createNewStationView();
            ScrapStationViews.append(stationView);
        }
    } else if(ScrapStationViews.size() > Note->scraps().size()) {
        //Remove stations from NoteStations
        int scrapToRemove = ScrapStationViews.size() - Note->scraps().size();
        //Add more stations to the NoteStations
        for(int i = 0; i < scrapToRemove; i++) {
            cwScrapStationView* deleteStationView = ScrapStationViews.last();
            ScrapStationViews.removeLast();
            deleteStationView->deleteLater();
        }
    }

    //Update stationview data
    for(int i = 0; i < ScrapStationViews.size(); i++) {
        updateScrapViewItem(i);
    }
}

/**
    Updates the scrap view item with new data
  */
void cwNoteStationView::updateScrapViewItem(int index) {
    Q_ASSERT(ScrapStationViews.size() == Note->scraps().size());
    cwScrapStationView* stationViewItem = ScrapStationViews[index];
    cwScrap* scrap = Note->scraps()[index];
    stationViewItem->setTransformUpdater(TransformUpdater);
    stationViewItem->setScrap(scrap);
}

/**
  Creates a new station view with the correct QML context
  */
cwScrapStationView *cwNoteStationView::createNewStationView() {
    cwScrapStationView* stationView = new cwScrapStationView(this);
    QDeclarativeContext* context = QDeclarativeEngine::contextForObject(this);
    QDeclarativeEngine::setContextForObject(stationView, context);
    return stationView;
}


