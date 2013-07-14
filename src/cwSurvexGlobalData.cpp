/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwDebug.h"

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

        //Add all equate stations to EquateMap
        populateEquateMap(currentBlock);

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
            fixStationNames(newChunk, currentBlock);
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
 * @param rootBlock - The block that's holds the chunk.
 *
 * This will take the current block and see if any of the stations in chunk, are in any of the
 * parent's equates.  If the station is in the equate, then the station will be renamed.
 */
void cwSurvexGlobalData::fixStationNames(cwSurveyChunk *chunk, cwSurvexBlockData *chunkBlock)
{
    cwSurvexBlockData* current = chunkBlock;

    //Move to block that has a name
    while(current != NULL &&
          current->name().isEmpty() &&
          current->importType() != cwSurvexBlockData::Cave)
    {
        current = current->parentBlock();
    }

    if(current != NULL) {
        chunkBlock = current;
    }

    QString stationPrefix;

    //Find the parent cave chuck block
    cwSurvexBlockData* caveSurvexBlock = chunkBlock;
    while(caveSurvexBlock != NULL) {
        if(caveSurvexBlock->importType() == cwSurvexBlockData::Cave) {
            break;
        } else {
            if(!caveSurvexBlock->name().isEmpty()) {
                stationPrefix.prepend(caveSurvexBlock->name() + ".");
            }
        }

        caveSurvexBlock = caveSurvexBlock->parentBlock();
    }

    //No cave block, just use the chunkblock, there's really only one trip
    if(caveSurvexBlock == NULL) {
        stationPrefix.clear();
        caveSurvexBlock = chunkBlock;
    }


    //Go through all the stations and fix the station names
    for(int i = 0; i < chunk->stationCount(); i++) {
        cwStation station = chunk->station(i); //removePeriodFromStationName(chunk->station(i));
        chunk->setStation(station, i);

        //See if the station is exported
        if(chunkBlock->ExportStations.contains(station.name())) {
            QString fullStationName = stationPrefix + station.name();

            //See if the station needs to be converted
            if(caveSurvexBlock->EquateMap.contains(fullStationName)) {
                //Chance the station name
                QString mappedStationName = caveSurvexBlock->EquateMap.value(fullStationName);

                station.setName(mappedStationName);
                chunk->setStation(station, i);

            } else {
                qDebug() << "Exported station" << fullStationName << "can't be found" << LOCATION;
            }
        } else {
            //See if the station exists for the know stations for the cave
            if(caveSurvexBlock->EquateMap.contains(station.name())) {

                //Station name already exist
                QString newStationName;

                //See if the station is already been used survey block
                if(chunkBlock->EquateMap.contains(station.name())) {
                    //Station name is good!
                    newStationName = chunkBlock->EquateMap.value(station.name());
                } else {
                    //Duplicate station name!
                    newStationName = generateUniqueStationName(station.name(), caveSurvexBlock);
                }

                station.setName(newStationName);
                chunk->setStation(station, i);

                //Update the station for the trip block
                chunkBlock->EquateMap.insert(station.name(), newStationName);

            } else {

                //This Remove extra "periods" if they exist. Allows parent
                //Structures to add shoots to child trips. (silly survex feature)
                QString fullStationName = station.name();
                QString newStationName = station.name().split(".").last();
                station.setName(newStationName);
                chunk->setStation(station, i);

                //Add the station to the station cave lookup
                //Doesn't exist in the cave or the current trip
                chunkBlock->EquateMap.insert(fullStationName, station.name());
                caveSurvexBlock->EquateMap.insert(fullStationName, station.name());
            }
        }
    }

    fixDuplicatedStationInShot(chunk, caveSurvexBlock);
}

/**
 * @brief cwSurvexGlobalData::fixDuplicatedStationInShot
 * @param chunk
 *
 * Because station name mangaling, sho
 */
void cwSurvexGlobalData::fixDuplicatedStationInShot(cwSurveyChunk *chunk, cwSurvexBlockData* caveSurvexBlock)
{
    for(int i = 0; i < chunk->stationCount(); i+=2) {
        cwStation station1 = chunk->station(i);
        cwStation station2 = chunk->station(i + 1);

        if(station1.name() == station2.name()) {
            //Rename one of the stations
            station1.setName(generateUniqueStationName(station1.name(), caveSurvexBlock));
            chunk->setStation(station1, i);
        }
    }
}

/**
 * @brief cwSurvexGlobalData::populateEquateMap
 * @param block
 *
 * This adds all the equate stations to the list
 */
void cwSurvexGlobalData::populateEquateMap(cwSurvexBlockData *block)
{
    foreach(QStringList equateList, block->EqualStations) {
        if(equateList.size() < 2) {
            continue;
        }

        //Find the best station
        QString bestStationName;
        foreach(QString name, equateList) {
            if(block->EquateMap.contains(name) && bestStationName.isEmpty()) {
                bestStationName = block->EquateMap.value(name);
                break;
//            } else if(block->EquateMap.contains(name)) {
                //There's 3 or more equates linking a bunch of stations together
                //Cavewhere can't handle this right now
                //FIXME: Allow for 3 or more equates
//                qDebug() << "Can't handle 3 or more equates with the same stations" << LOCATION;
            }
        }

        if(bestStationName.isEmpty()) {
            bestStationName = equateList.first();
        }

        bestStationName = bestStationName.split(".").last();

        foreach(QString name, equateList) {
            block->EquateMap.insert(name, bestStationName);
        }
    }

}

/**
 * @brief cwSurvexGlobalData::newStationName
 * @param oldStationName
 * @return This will add 'x' to the end oldStationName until it is unique to the cave
 */
QString cwSurvexGlobalData::generateUniqueStationName(QString oldStationName, cwSurvexBlockData* caveSurvexBlock) const
{
    QString newStationName = oldStationName + "x";
    while(caveSurvexBlock->EquateMap.contains(newStationName)) {
        newStationName = newStationName + "x";
    }
    return newStationName;
}
