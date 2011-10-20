//Our includes
#include "cwBaseNoteStationInteraction.h"
#include "cwNoteStation.h"
#include "cwNoteStationView.h"
#include "cwScrap.h"

cwBaseNoteStationInteraction::cwBaseNoteStationInteraction(QDeclarativeItem *parent) :
    cwBasePanZoomInteraction(parent),
    Scrap(NULL),
    NoteStationView(NULL)
{
}

/**
  \brief Adds a new station to the cwNoteItem

  The new station position will be converted from qtViewportCoordinates into
  a normalized position of the notes.

  \param qtViewportCoordinates - This is the unprojected viewport coordinates, ie
  the qt window coordinates were the top left is the origin.  These should be
  in local coordines of the cwNoteItem

  */
void  cwBaseNoteStationInteraction::addStation(QPointF notePosition) {
    qDebug() << "Add station!" << notePosition;

    if(Scrap == NULL) { return; }

    cwNoteStation newNoteStation;
    newNoteStation.setPositionOnNote(notePosition);

    //Try to guess the station name
    QString stationName;
    if(NoteStationView != NULL) {
        cwNoteStation selectedStation = NoteStationView->selectNoteStation();
        stationName = Scrap->guessNeighborStationName(selectedStation, notePosition);
    }

    if(stationName.isEmpty()) {
        stationName = "Station Name";
    }

    newNoteStation.station().setName(stationName);
    Scrap->addStation(newNoteStation);

    //Get the last station in the list and select it
    if(NoteStationView != NULL) {
        NoteStationView->selectNoteStation(Scrap->numberOfStations());
    }
}

/**
Called when the scrap has changed
*/
void cwBaseNoteStationInteraction::setScrap(cwScrap* scrap) {
    if(Scrap != scrap) {
        Scrap = scrap;
        emit scrapChanged();
    }
}

/**
  Sets the note station view
  */
void cwBaseNoteStationInteraction::setNoteStationView(cwNoteStationView* stationView) {
    if(NoteStationView != stationView) {
        NoteStationView = stationView;
        emit noteStationViewChanged();
    }
}
