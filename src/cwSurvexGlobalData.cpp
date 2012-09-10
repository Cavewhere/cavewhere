#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

cwSurvexGlobalData::cwSurvexGlobalData(QObject* parent) :
    QObject(parent)
{

}

void cwSurvexGlobalData::setBlocks(QList<cwSurvexBlockData*> blocks) {
    foreach(cwSurvexBlockData* block, blocks) {
        block->setParent(this);
        block->setParentBlock(NULL);
    }

    RootBlocks = blocks;
}

/**
  \brief Get's all the cave data from this object
  */
QList<cwCave*> cwSurvexGlobalData::caves() {
    QList<cwCave*> caves;
    foreach(cwSurvexBlockData* block, blocks()) {
        cavesHelper(&caves, block, NULL, NULL);
    }

    return caves;
}

/**
  \brief Get's the current errors
  */
QStringList cwSurvexGlobalData::erros() {
    return QStringList();
}


/**
  \brief Helper function the caves function
  */
void cwSurvexGlobalData::cavesHelper(QList<cwCave*>* caves,
                                     cwSurvexBlockData* currentBlock,
                                     cwCave* currentCave,
                                     cwTrip* currentTrip) {

    switch(currentBlock->importType()) {
    case cwSurvexBlockData::NoImport:
        break;
    case cwSurvexBlockData::Cave:
        currentCave = new cwCave(this);

        currentCave->setName(currentBlock->name());
        currentTrip = NULL;
        caves->append(currentCave);
        break;
    case cwSurvexBlockData::Trip: {
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
    case cwSurvexBlockData::Structure:
        break;
    }

    //Only import real data
    if(currentBlock->importType() != cwSurvexBlockData::NoImport) {

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

            //Fix station that need to be Equated
            fixEquatedStationNames(newChunk, currentBlock);
        }
    }

    //Recusive call
    foreach(cwSurvexBlockData* childBlock, currentBlock->childBlocks()) {
        cavesHelper(caves, childBlock, currentCave, currentTrip);
    }
}

/**
 * @brief cwSurvexGlobalData::fixEquatedStationNames
 * @param chunk
 * @param currentBlock
 *
 * This will take the current block and see if any of the stations in chunk, are in any of the
 * parent's equates.  If the station is in the equate, then the station will be renamed.
 */
void cwSurvexGlobalData::fixEquatedStationNames(cwSurveyChunk *chunk, cwSurvexBlockData *currentBlock)
{
    for(int i = 0; i < chunk->stationCount(); i++) {
        cwStation station = chunk->station(i);

        QString fullStationName = station.name();
        if(!currentBlock->name().isEmpty()) {
            fullStationName.prepend(currentBlock->name() + ".");
        }

        QStringList fullStationNameList;
        fullStationNameList.append(fullStationName);

        //While the currentBlock is trip or structure and the parent is null
        while((currentBlock->importType() == cwSurvexBlockData::Trip ||
              currentBlock->importType() == cwSurvexBlockData::Structure)
              &&
              currentBlock->parentBlock() != NULL)
        {
            currentBlock = currentBlock->parentBlock();

            for

            //What the station is equal to
            QString fullEquateStationName = currentBlock->equatedStation(fullStationName);
            QStringList brokenDownNameList = fullEquateStationName.split('.').last();

            //Make sure we found a name.  If the current station isn't equated to anything
            //the name will not be updated.
            if(!brokenDownNameList.isEmpty()) {
                QString newStationName = brokenDownNameList.last();

                //If the station name is different, the
                if(newStationName != station.name()) {
                    station.setName(newStationName);
                    chunk->setStation(station, i);

                    //Update the fullStationName with the Equate station name
                    fullStationName = fullEquateStationName;
                }
            }
        }
    }
}
