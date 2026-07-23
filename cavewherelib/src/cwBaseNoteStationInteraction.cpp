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
#include "cwTrip.h"

namespace {
//! First solved station in the scrap's trip, by scope-relative name — the same
//! list the scope autocomplete offers. Pre-filling a new station with it saves
//! typing when the guess and this agree; falls back to a placeholder when the
//! trip is unsolved or absent.
QString defaultStationName(cwScrap* scrap)
{
    cwTrip* trip = scrap != nullptr ? scrap->parentTrip() : nullptr;
    if (trip != nullptr) {
        const QList<QPair<QString, QVector3D>> solved = trip->solvedStations();
        if (!solved.isEmpty()) {
            return solved.first().first;
        }
    }
    return QStringLiteral("Station Name");
}
}

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
        stationName = defaultStationName(scrap);
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

