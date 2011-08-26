#include "cwWallsGlobalData.h"
#include "cwWallsBlockData.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

cwWallsGlobalData::cwWallsGlobalData(QObject* parent) :
    QObject(parent)
{

}

void cwWallsGlobalData::setBlocks(QList<cwWallsBlockData*> blocks) {
    foreach(cwWallsBlockData* block, blocks) {
        block->setParent(this);
        block->setParentBlock(NULL);
    }

    RootBlocks = blocks;
}

/**
  \brief Get's all the cave data from this object
  */
QList<cwCave*> cwWallsGlobalData::caves() {
    QList<cwCave*> caves;
    foreach(cwWallsBlockData* block, blocks()) {
        cavesHelper(&caves, block, NULL, NULL);
    }

    return caves;
}

/**
  \brief Get's the current errors
  */
QStringList cwWallsGlobalData::erros() {
    return QStringList();
}


/**
  \brief Helper function the caves function
  */
void cwWallsGlobalData::cavesHelper(QList<cwCave*>* caves,
                                     cwWallsBlockData* currentBlock,
                                     cwCave* currentCave,
                                     cwTrip* currentTrip) {

    switch(currentBlock->importType()) {
    case cwWallsBlockData::NoImport:
        break;
    case cwWallsBlockData::Cave:
        currentCave = new cwCave(this);

        currentCave->setName(currentBlock->name());
        currentTrip = NULL;
        caves->append(currentCave);
        break;
    case cwWallsBlockData::Trip: {
        currentTrip = new cwTrip(this);

        //Copy the name and date
        currentTrip->setName(currentBlock->name());
        currentTrip->setDate(currentBlock->date());

        //Copy all the team members
        currentTrip->setTeam(new cwTeam(*(currentBlock->team()))); //Copy the team

        //Copy the calibration
        currentTrip->setCalibration(new cwTripCalibration(*(currentBlock->calibration())));

        //Creates a cave for the trip if there isn't one
        if(currentCave == NULL) {
            currentCave = new cwCave(this);
            currentCave->setName(QString("Cave %1").arg(caves->size() + 1));
            caves->append(currentCave);
        }

        //Sets the trip name
        QString tripName = currentBlock->name();
        if(tripName.isEmpty()) {
            tripName = QString("Trip %1").arg(currentCave->tripCount() + 1);
        }
        currentTrip->setName(tripName);

        //Adds the trip to the current cave
        currentCave->addTrip(currentTrip);
    }
    case cwWallsBlockData::Structure:
        break;
    }

    //Only import real data
    if(currentBlock->importType() != cwWallsBlockData::NoImport) {

        //Make sure we have trip if there's chunks
        if(currentBlock->chunkCount() > 0 && currentTrip == NULL) {

            //Make sure we have a cave
            if(currentCave == NULL) {
                currentCave = new cwCave(this);
                currentCave->setName(QString("Cave %1").arg(caves->size() + 1));
                caves->append(currentCave);
            }

            //Create the trip
            currentTrip = new cwTrip(this);
            currentTrip->setName(QString("Trip %1").arg(currentCave->tripCount() + 1));
            currentCave->addTrip(currentTrip);
        }

        //Add all the chunks to the
        foreach(cwSurveyChunk* chunk, currentBlock->chunks()) {
            cwSurveyChunk* newChunk = new cwSurveyChunk(*chunk);
            currentTrip->addChunk(newChunk);
        }
    }

    //Recusive call
    foreach(cwWallsBlockData* childBlock, currentBlock->childBlocks()) {
        cavesHelper(caves, childBlock, currentCave, currentTrip);
    }
}
