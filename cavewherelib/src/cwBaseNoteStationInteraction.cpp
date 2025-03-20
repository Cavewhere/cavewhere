/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBaseNoteStationInteraction.h"
#include "cwNoteStation.h"
//#include "cwNoteStationView.h"
#include "cwScrap.h"
#include "cwScrapView.h"
#include "cwScrapItem.h"
#include "cwScrapStationView.h"

cwBaseNoteStationInteraction::cwBaseNoteStationInteraction(QQuickItem *parent) :
    cwBaseNotePointInteraction(parent)
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
void  cwBaseNoteStationInteraction::addPoint(QPointF notePosition, cwScrapItem* scrapItem) {
    cwScrap* scrap = scrapItem->scrap();

    cwNoteStation newNoteStation;
    newNoteStation.setPositionOnNote(notePosition);

    //Try to guess the station name
    cwNoteStation selectedStation = scrapItem->stationView()->selectedNoteStation();
    QString stationName = scrap->guessNeighborStationName(selectedStation, notePosition);

    if(stationName.isEmpty()) {
        stationName = "Station Name";
    }

    newNoteStation.setName(stationName);
    scrap->addStation(newNoteStation);

    //Get the last station in the list and select it
    scrapItem->stationView()->setSelectedItemIndex(scrap->numberOfStations() - 1);

    //If this is the first item, we'll need to make it go into edit mode
    if(scrap->numberOfStations() == 1) {
        QQuickItem* stationItem = scrapItem->stationView()->selectedItem();
        QMetaObject::invokeMethod(stationItem, "startEditting");
    }
}

