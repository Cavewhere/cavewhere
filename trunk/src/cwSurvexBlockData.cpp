//Our includes
#include "cwSurvexBlockData.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"

cwSurvexBlockData::cwSurvexBlockData(QObject* parent) :
    QObject(parent),
    ParentBlock(NULL),
    Type(NoImport)
{
}

/**
  \brief Gets the survex block at index
  */
 cwSurvexBlockData* cwSurvexBlockData::childBlock(int index) {
     return ChildBlocks[index];
 }

/**
  \brief Get's the chunk at index
  */
 cwSurveyChunk* cwSurvexBlockData::chunk(int index) {
     return Chunks[index];
 }

 /**
   \brief Sets the name of the block data
   */
 void cwSurvexBlockData::setName(QString name) {
     if(Name != name) {
         Name = name;
         emit nameChanged();
     }
 }

 /**
   \brief Sets how this block will be exported
   */
 void cwSurvexBlockData::setImportType(ImportType type) {
     if(Type != type) {
         //Go through children and update thier type
         Type = type;
         emit importTypeChanged();

         foreach(cwSurvexBlockData* child, ChildBlocks) {
             if(Type != NoImport) {
                 if(child->isTrip()) {
                     child->setImportType(Trip);
                 } else {
                     child->setImportType(Structure);
                 }
             } else {
                 child->setImportType(NoImport);
             }
         }
     }
 }

 /**
   \brief Checks to see if this is a trip
   */
 bool cwSurvexBlockData::isTrip() const {
     cwSurvexBlockData* parent = parentBlock();
     while(parent != NULL) {
         if(parent->importType() == Trip) {
             return false;
         }
         parent = parent->parentBlock();
     }

     return !Chunks.isEmpty();
 }

 /**
   \brief Converts the export type to a string
   */
 QString cwSurvexBlockData::importTypeToString(ImportType type) {
     switch(type) {
     case NoImport:
         return QString("Don't Import");
     case Cave:
         return QString("Cave");
     case Trip:
         return QString("Trip");
     default:
         break;
     }
     return QString();
 }

 /**
   \brief Adds a child block to the data
   */
 void cwSurvexBlockData::addChildBlock(cwSurvexBlockData* blockData) {
     blockData->ParentBlock = this;
     ChildBlocks.append(blockData);
 }

 /**
   \brief Adds a chunk to the data
   */
 void cwSurvexBlockData::addChunk(cwSurveyChunk* chunk) {
     chunk->setParent(this);
     Chunks.append(chunk);
 }

 /**
   \brief Clears the data from this object

   This will delete the block, if they're still owned by this object
   */
 void cwSurvexBlockData::clear() {
     Chunks.clear();
     ChildBlocks.clear();
 }

 /**
   \brief Get's all the station for this survex block
   */
 int cwSurvexBlockData::stationCount() const {
     int numStations = 0;
     foreach(cwSurveyChunk* chunk, Chunks) {
         numStations += chunk->StationCount();
     }
     return numStations;
 }

 /**
   \brief Gets the station at index

   Returns null if the index is out of bounds. The index is out of bound when index >=
   stationCount()
   */
 cwStationReference* cwSurvexBlockData::station(int index) const {
     foreach(cwSurveyChunk* chunk, Chunks) {
         if(index < chunk->StationCount()) {
             return chunk->Station(index);
         }
         index -= chunk->StationCount();
     }
     return NULL;
 }

/**
  \brief Gets the number of shots in the block
  */
 int cwSurvexBlockData::shotCount() const {
     int numShots = 0;
     foreach(cwSurveyChunk* chunk, Chunks) {
         numShots += chunk->ShotCount();
     }
     return numShots;
 }

 /**
   \brief Gets the shot at an index

   \param index - the index of the shot
   */
 cwShot* cwSurvexBlockData::shot(int index) const {
     foreach(cwSurveyChunk* chunk, Chunks) {
         if(index < chunk->ShotCount()) {
             return chunk->Shot(index);
         }
         index -= chunk->ShotCount();
     }
     return NULL;
 }

 /**
   \brief Get's the index of the shot

   If the shot doesn't in this block, then -1 is return
   */
 int cwSurvexBlockData::indexOfShot(cwShot* shot) const {
     int index = 0;
     foreach(cwSurveyChunk* chunk, Chunks) {
         for(int i = 0; i < chunk->ShotCount(); i++) {
             cwShot* chunkShot = chunk->Shot(i);
             if(shot == chunkShot) {
                 return index;
             }
             index++;
         }
     }
     return -1;
 }

