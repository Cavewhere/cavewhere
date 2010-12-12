//Our includes
#include "cwSurvexBlockData.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"

cwSurvexBlockData::cwSurvexBlockData(QObject* parent) :
    QObject(parent),
    ParentBlock(NULL)
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
 void cwSurvexBlockData::setBlockName(QString name) {
     Name = name;
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
 int cwSurvexBlockData::stationCount() {
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
 cwStation* cwSurvexBlockData::station(int index) {
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
 int cwSurvexBlockData::shotCount() {
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
 cwShot* cwSurvexBlockData::shot(int index) {
     foreach(cwSurveyChunk* chunk, Chunks) {
         if(index < chunk->ShotCount()) {
             return chunk->Shot(index);
         }
         index -= chunk->ShotCount();
     }
     return NULL;
 }

