#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"
#include "cwCave.h"
#include "cwSurveyTrip.h"
#include "cwSurveyChunk.h"

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
void cwSurvexGlobalData::cavesHelper(QList<cwCave*>* caves, cwSurvexBlockData* currentBlock, cwCave* currentCave, cwSurveyTrip* currentTrip) {

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
        currentTrip = new cwSurveyTrip(this);

        currentTrip->setName(currentBlock->name());
        if(currentCave == NULL) {
            currentCave = new cwCave(this);
            currentCave->setName(QString("Cave %1").arg(caves->size() + 1));
            caves->append(currentCave);
        }

        QString tripName = currentBlock->name();
        if(tripName.isEmpty()) {
            tripName = QString("Trip %1").arg(currentCave->tripCount() + 1);
        }
        currentTrip->setName(tripName);

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
            currentTrip = new cwSurveyTrip(this);
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
    foreach(cwSurvexBlockData* childBlock, currentBlock->childBlocks()) {
        cavesHelper(caves, childBlock, currentCave, currentTrip);
    }
}
