//Our includes
#include "cwWallsBlockData.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

cwWallsBlockData::cwWallsBlockData(QObject* parent) :
    QObject(parent),
    ParentBlock(NULL),
    Type(NoImport),
    Team(new cwTeam(this)),
    Calibration(new cwTripCalibration(this))
{
}

/**
  \brief Gets the Walls block at index
  */
 cwWallsBlockData* cwWallsBlockData::childBlock(int index) {
     return ChildBlocks[index];
 }

/**
  \brief Get's the chunk at index
  */
 cwSurveyChunk* cwWallsBlockData::chunk(int index) {
     return Chunks[index];
 }

 /**
   \brief Sets the name of the block data
   */
 void cwWallsBlockData::setName(QString name) {
     if(Name != name) {
         Name = name;
         emit nameChanged();
     }
 }

 /**
   \brief Sets how this block will be exported
   */
 void cwWallsBlockData::setImportType(ImportType type) {
     if(Type != type) {
         //Go through children and update thier type
         Type = type;
         emit importTypeChanged();

         foreach(cwWallsBlockData* child, ChildBlocks) {
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
 bool cwWallsBlockData::isTrip() const {
     cwWallsBlockData* parent = parentBlock();
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
 QString cwWallsBlockData::importTypeToString(ImportType type) {
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
 void cwWallsBlockData::addChildBlock(cwWallsBlockData* blockData) {
     blockData->ParentBlock = this;
     ChildBlocks.append(blockData);
 }

 /**
   \brief Adds a chunk to the data
   */
 void cwWallsBlockData::addChunk(cwSurveyChunk* chunk) {
     chunk->setParent(this);
     Chunks.append(chunk);
 }

 /**
   \brief Clears the data from this object

   This will delete the block, if they're still owned by this object
   */
 void cwWallsBlockData::clear() {
     Chunks.clear();
     ChildBlocks.clear();
 }

 /**
   \brief Get's all the station for this Walls block
   */
 int cwWallsBlockData::stationCount() const {
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
 cwStationReference cwWallsBlockData::station(int index) const {
     foreach(cwSurveyChunk* chunk, Chunks) {
         if(index < chunk->StationCount()) {
             return chunk->Station(index);
         }
         index -= chunk->StationCount();
     }
     return cwStationReference();
 }

/**
  \brief Gets the number of shots in the block
  */
 int cwWallsBlockData::shotCount() const {
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
 cwShot* cwWallsBlockData::shot(int index) const {
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
 int cwWallsBlockData::indexOfShot(cwShot* shot) const {
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

