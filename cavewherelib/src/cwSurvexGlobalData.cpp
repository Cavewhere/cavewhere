/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
#include "cwSurvexGlobalData.h"
#include "cwTreeImportDataNode.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwDebug.h"
#include "cwSurvexNodeData.h"

//Qt includes
#include <QThread>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QStringList>

cwSurvexGlobalData::cwSurvexGlobalData(QObject* parent) :
    cwTreeImportData(parent), NodeData()
{

}

cwSurvexNodeData* cwSurvexGlobalData::nodeData(cwTreeImportDataNode *node)
{
    if (!NodeData.contains(node)) {
        cwSurvexNodeData* result = new cwSurvexNodeData(node);
        NodeData[node] = result;
    }
    return NodeData[node];
}

/**
  \brief Get's all the cave data from this object
  */
QList<cwCave*> cwSurvexGlobalData::caves() {
    QList<cwCave*> caves;
    foreach(cwTreeImportDataNode* block, nodes()) {
        cavesHelper(&caves, block, nullptr, nullptr);
    }

    return caves;
}

/**
  \brief Helper function the caves function
  */
void cwSurvexGlobalData::cavesHelper(QList<cwCave*>* caves,
                                     cwTreeImportDataNode* currentBlock,
                                     cwCave* currentCave,
                                     cwTrip* currentTrip) {

    switch(currentBlock->importType()) {
    case cwTreeImportDataNode::NoImport:
        break;
    case cwTreeImportDataNode::Cave:
        currentCave = new cwCave(this);

        currentCave->setName(currentBlock->displayName());
        currentTrip = nullptr;
        caves->append(currentCave);

        //Add all equate stations to EquateMap
        populateEquateMap(currentBlock);

        break;
    case cwTreeImportDataNode::Trip: {
        currentTrip = new cwTrip(this);

        //Copy the name and date
        currentTrip->setName(currentBlock->displayName());
        currentTrip->setDate(QDateTime(currentBlock->date(), QTime()));

        //Copy all the team members
        currentTrip->team()->setData(currentBlock->team()->data());

        //Copy the calibration
        currentTrip->calibrations()->setData(currentBlock->calibration()->data());
        // currentTrip->setCalibration(new cwTripCalibration(*(currentBlock->calibration())));

        //Creates a cave for the trip if there isn't one
        if(currentCave == nullptr) {
            currentCave = new cwCave(this);
            currentCave->setName(QString("Cave %1").arg(caves->size() + 1));
            caves->append(currentCave);
        }

        //Sets the trip name
        QString tripName = currentBlock->displayName();
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
            cwSurveyChunk* newChunk = new cwSurveyChunk();
            newChunk->setData(chunk->data());
            currentTrip->addChunk(newChunk);

            //Fix station that need to be Equated
            fixStationNames(newChunk, currentBlock);
        }
    }

    //Recusive call
    foreach(cwTreeImportDataNode* childBlock, currentBlock->childNodes()) {
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
void cwSurvexGlobalData::fixStationNames(cwSurveyChunk *chunk, cwTreeImportDataNode *chunkBlock)
{
    cwTreeImportDataNode* current = chunkBlock;

    //Move to block that has a name
    while(current != nullptr &&
          current->name().isEmpty() &&
          current->importType() != cwTreeImportDataNode::Cave)
    {
        current = current->parentNode();
    }

    if(current != nullptr) {
        chunkBlock = current;
    }
    auto chunkData = nodeData(chunkBlock);

    QString stationPrefix;

    //Find the parent cave chuck block
    cwTreeImportDataNode* caveSurvexBlock = chunkBlock;
    while(caveSurvexBlock != nullptr) {
        if(caveSurvexBlock->importType() == cwTreeImportDataNode::Cave) {
            break;
        } else {
            if(!caveSurvexBlock->name().isEmpty()) {
                stationPrefix.prepend(caveSurvexBlock->name() + ".");
            }
        }

        caveSurvexBlock = caveSurvexBlock->parentNode();
    }

    //No cave block, just use the chunkblock, there's really only one trip
    if(caveSurvexBlock == nullptr) {
        stationPrefix.clear();
        caveSurvexBlock = chunkBlock;
    }

    auto caveSurvexData = nodeData(caveSurvexBlock);


    //Go through all the stations and fix the station names
    for(int i = 0; i < chunk->stationCount(); i++) {
        cwStation station = chunk->station(i); //removePeriodFromStationName(chunk->station(i));
        chunk->setStation(station, i);

        //See if the station is exported
        if(chunkData->ExportStations.contains(station.name())) {
            QString fullStationName = stationPrefix + station.name();

            //See if the station needs to be converted
            if(caveSurvexData->EquateMap.contains(fullStationName)) {
                //Chance the station name
                QString mappedStationName = caveSurvexData->EquateMap.value(fullStationName);

                station.setName(mappedStationName);
                chunk->setStation(station, i);

            } else {
                qDebug() << "Exported station" << fullStationName << "can't be found" << LOCATION;
            }
        } else {
            //See if the station exists for the know stations for the cave
            if(caveSurvexData->EquateMap.contains(station.name())) {

                //Station name already exist
                QString newStationName;

                //See if the station is already been used survey block
                if(chunkData->EquateMap.contains(station.name())) {
                    //Station name is good!
                    newStationName = chunkData->EquateMap.value(station.name());
                } else {
                    //Duplicate station name!
                    newStationName = generateUniqueStationName(station.name(), caveSurvexBlock);
                }

                station.setName(newStationName);
                chunk->setStation(station, i);

                //Update the station for the trip block
                chunkData->EquateMap.insert(station.name(), newStationName);

            } else {

                //This Remove extra "periods" if they exist. Allows parent
                //Structures to add shoots to child trips. (silly survex feature)
                QString fullStationName = station.name();
                QString newStationName = station.name().split(".").last();
                station.setName(newStationName);
                chunk->setStation(station, i);

                //Add the station to the station cave lookup
                //Doesn't exist in the cave or the current trip
                chunkData->EquateMap.insert(fullStationName, station.name());
                caveSurvexData->EquateMap.insert(fullStationName, station.name());
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
void cwSurvexGlobalData::fixDuplicatedStationInShot(cwSurveyChunk *chunk, cwTreeImportDataNode* caveSurvexBlock)
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
void cwSurvexGlobalData::populateEquateMap(cwTreeImportDataNode *block)
{
    auto blockData = nodeData(block);

    //Collapse every *equate in this block by transitive closure (union-find)
    //instead of per-statement, so a chain of 2-way ties — *equate A B;
    //*equate B C; *equate C D — merges to one representative name exactly like a
    //single N-way *equate A B C D would. The previous per-statement merge only
    //looked at one statement at a time and left transitively-equated stations
    //under different names (the old 3+-way FIXME).
    QHash<QString, QString> parent; //name -> parent in the union-find forest

    //Names absent from `parent` are their own root, so find() returns them
    //unchanged and unite() only has to relink the two roots.
    const auto find = [&parent](QString name) -> QString {
        QString root = name;
        while (parent.value(root, root) != root) {
            root = parent.value(root, root);
        }
        while (name != root) {
            const QString next = parent.value(name, name);
            parent.insert(name, root);
            name = next;
        }
        return root;
    };

    const auto unite = [&parent, &find](const QString& a, const QString& b) {
        const QString rootA = find(a);
        const QString rootB = find(b);
        if (rootA != rootB) {
            parent.insert(rootB, rootA);
        }
    };

    //Keep first-seen order so the representative choice is deterministic.
    QStringList order;
    QSet<QString> seen;
    const auto note = [&order, &seen](const QString& name) {
        if (!seen.contains(name)) {
            seen.insert(name);
            order.append(name);
        }
    };

    for (const QStringList& equateList : blockData->EqualStations) {
        if (equateList.size() < 2) {
            continue;
        }
        note(equateList.first());
        for (int i = 1; i < equateList.size(); ++i) {
            note(equateList.at(i));
            unite(equateList.first(), equateList.at(i));
        }
    }

    //A name already carrying a mapping is an existing/known station; prefer it
    //as the representative for its set, matching the old "best station" bias.
    const QMap<QString, QString> preExisting = blockData->EquateMap;

    QHash<QString, QString> representativeForRoot; //root -> chosen full name
    for (const QString& name : order) {
        const QString root = find(name);
        if (!representativeForRoot.contains(root)
                || (preExisting.contains(name)
                    && !preExisting.contains(representativeForRoot.value(root)))) {
            representativeForRoot.insert(root, name);
        }
    }

    for (const QString& name : order) {
        const QString root = find(name);
        QString base = representativeForRoot.value(root);
        if (preExisting.contains(base)) {
            base = preExisting.value(base);
        }
        const QString bestStationName = base.split(QLatin1Char('.')).last();
        blockData->EquateMap.insert(name, bestStationName);
    }
}

/**
 * @brief cwSurvexGlobalData::newStationName
 * @param oldStationName
 * @return This will add 'x' to the end oldStationName until it is unique to the cave
 */
QString cwSurvexGlobalData::generateUniqueStationName(QString oldStationName, cwTreeImportDataNode* caveSurvexBlock)
{
    QString newStationName = oldStationName + "x";
    auto caveSurvexData = nodeData(caveSurvexBlock);
    while(caveSurvexData->EquateMap.contains(newStationName)) {
        newStationName = newStationName + "x";
    }
    return newStationName;
}
