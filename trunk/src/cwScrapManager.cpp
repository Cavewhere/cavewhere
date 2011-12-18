//Our includes
#include "cwScrapManager.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"

cwScrapManager::cwScrapManager(QObject *parent) :
    QObject(parent),
    Region(NULL)
{

}

/**
  \brief Set the region for the scrap manager
  */
void cwScrapManager::setRegion(cwCavingRegion* region) {
    if(Region != NULL) {
        disconnect(Region, NULL, this, NULL);
    }

    Region = region;

    if(Region != NULL) {
        //Connect all signal from the region
        connect(Region, SIGNAL(destroyed(QObject*)), SLOT(regionDestroyed(QObject*)));
        connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(updateCaveScrapGeometry(int, int)));
        connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(connectAddedCaves(int,int)));
        connect(Region, SIGNAL(removedCaves(int,int)), SLOT(updateCaveScrapGeometry(int, int)));

        connectAllCaves();
    }
}

void cwScrapManager::connectAllCaves() {
    foreach(cwCave* cave, Region->caves()) {
        connectCave(cave);
    }
}

void cwScrapManager::connectCave(cwCave *cave) {
    connect(cave, SIGNAL(insertTrips(int,int)), SLOT(tripsInserted(int,int)));
    connect(cave, SIGNAL(beginRemoveTrips(int,int)), SLOT(tripsRemoved(int,int)));

    foreach(cwTrip* trip, cave->trips()) {
        connectTrip(trip);
    }
}

void cwScrapManager::connectTrip(cwTrip *trip) {
    connectNoteModel(trip->notes());
}

void cwScrapManager::connectNoteModel(cwSurveyNoteModel *noteModel) {
    connect(noteModel, SIGNAL(rowsAboutToBeInserted(QModelIndex, int, int)), SLOT(notesInserted(QModelIndex,int,int)));
    connect(noteModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)), SLOT(notesRemoved(QModelIndex,int,int)));

    foreach(cwNote* note, noteModel->notes()) {
        connectNote(note);
    }
}

void cwScrapManager::connectNote(cwNote *note) {
    connect(note, SIGNAL(insertedScraps(int,int)), SLOT(scrapInserted(int,int)));
    connect(note, SIGNAL(beginRemovingScraps(int,int)), SLOT(scrapRemoved(int,int)));

    connectScrapes(note->scraps());
}

void cwScrapManager::connectScrapes(QList<cwScrap*> scraps) {

    //Add all the scraps to the set
    foreach(cwScrap* scrap, scraps) {
        Scraps.insert(scrap);
    }

    //Update all the scrap geometry for the scrap
    updateScrapGeometry(scraps);
}

/**
  This function will run a task that will
  1. Crop out the scrap from the note, with mimap levels
  2. Generate geometry for the scrap
  3. Morph the geometry to the station positions
  */
void cwScrapManager::updateScrapGeometry(QList<cwScrap *> scraps) {


}

void cwScrapManager::cavesInserted(int begin, int end) {

}

void cwScrapManager::cavesRemoved(int begin, int end) {

}

void cwScrapManager::tripsInserted(int begin, int end) {

}

void cwScrapManager::tripsRemoved(int begin, int end) {

}

void cwScrapManager::notesInserted(QModelIndex parent, int begin, int end) {

}

void cwScrapManager::notesRemoved(QModelIndex parent, int begin, int end) {

}

void cwScrapManager::scrapInserted(int begin, int end) {

}

void cwScrapManager::scrapRemoved(int begin, int end) {

}

void cwScrapManager::updateScrapPoints() {

}
