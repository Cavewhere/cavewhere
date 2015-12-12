#include "cwWallsImportData.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"

cwWallsImportData::cwWallsImportData(QObject* parent)
    : cwTreeImportData(parent)
{

}

/**
  \brief Get's all the cave data from this object
  */
QList<cwCave*> cwWallsImportData::caves() {
    QList<cwCave*> caves;
    foreach(cwTreeImportDataNode* block, nodes()) {
        cavesHelper(&caves, block, nullptr, nullptr);
    }

    return caves;
}

/**
  \brief Helper function the caves function
  */
void cwWallsImportData::cavesHelper(QList<cwCave*>* caves,
                                     cwTreeImportDataNode* currentBlock,
                                     cwCave* currentCave,
                                     cwTrip* currentTrip) {

    switch(currentBlock->importType()) {
    case cwTreeImportDataNode::NoImport:
        break;
    case cwTreeImportDataNode::Cave:
        currentCave = new cwCave(this);

        currentCave->setName(currentBlock->name());
        currentTrip = nullptr;
        caves->append(currentCave);

        break;
    case cwTreeImportDataNode::Trip: {
        currentTrip = new cwTrip(this);

        //Copy the name and date
        currentTrip->setName(currentBlock->name());
        currentTrip->setDate(currentBlock->date());

        //Copy all the team members
        currentTrip->setTeam(new cwTeam(*(currentBlock->team()))); //Copy the team

        //Copy the calibration
        currentTrip->setCalibration(new cwTripCalibration(*(currentBlock->calibration())));

        //Creates a cave for the trip if there isn't one
        if(currentCave == nullptr) {
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
    case cwTreeImportDataNode::Structure:
        break;
    }

    //Only import real data
    if(currentBlock->importType() != cwTreeImportDataNode::NoImport) {

        //Make sure we have trip if there's chunks
        if(currentBlock->chunkCount() > 0 && currentTrip == nullptr) {

            //Make sure we have a cave
            if(currentCave == nullptr) {
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
    foreach(cwTreeImportDataNode* childBlock, currentBlock->childNodes()) {
        cavesHelper(caves, childBlock, currentCave, currentTrip);
    }
}
