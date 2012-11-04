//Our includes
#include "cwSurvexBlockData.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

cwSurvexBlockData::cwSurvexBlockData(QObject* parent) :
    QObject(parent),
    ParentBlock(NULL),
    Type(NoImport),
    Team(new cwTeam(this)),
    Calibration(new cwTripCalibration(this)),
    IncludeDistance(true)
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
  * @brief cwSurvexBlockData::addLRUDChunk
  * This creates a new LRUD chunk
  */
 void cwSurvexBlockData::addLRUDChunk()
 {
     LRUDChunks.append(cwSurvexLRUDChunk());
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
         numStations += chunk->stationCount();
     }
     return numStations;
 }

 /**
   \brief Gets the station at index

   Returns null if the index is out of bounds. The index is out of bound when index >=
   stationCount()
   */
 cwStation cwSurvexBlockData::station(int index) const {
     foreach(cwSurveyChunk* chunk, Chunks) {
         if(index < chunk->stationCount()) {
             return chunk->station(index);
         }
         index -= chunk->stationCount();
     }
     return cwStation();
 }

/**
  \brief Gets the number of shots in the block
  */
 int cwSurvexBlockData::shotCount() const {
     int numShots = 0;
     foreach(cwSurveyChunk* chunk, Chunks) {
         numShots += chunk->shotCount();
     }
     return numShots;
 }

 /**
   Get's the parent cwSurveyChunk at shot index
   */
 cwSurveyChunk *cwSurvexBlockData::parentChunkOfShot(int shotIndex) const {
     foreach(cwSurveyChunk* chunk, Chunks) {
         if(shotIndex < chunk->shotCount()) {
             return chunk;
         }
         shotIndex -= chunk->shotCount();
     }
     return NULL;
 }

 int cwSurvexBlockData::chunkShotIndex(int shotIndex) const
 {
     foreach(cwSurveyChunk* chunk, Chunks) {
         if(shotIndex < chunk->shotCount()) {
             return shotIndex;
         }
         shotIndex -= chunk->shotCount();
     }
     return -1;
 }


 /**
  * @brief cwSurvexBlockData::equatedStations
  * @return
  */
 QStringList cwSurvexBlockData::equatedStations(QString stationName) const
 {
     QStringList equatedStations;
     foreach(QStringList stations, EqualStations) {
         if(stations.contains(stationName)) {
             equatedStations.append(stations);
         }
     }

     return equatedStations;
 }

 /**
  * @brief cwSurvexBlockData::addExportStations
  * @param exportStations
  *
  * Appends the export stations to the export station list
  */
 void cwSurvexBlockData::addExportStations(QStringList exportStations)
 {
     foreach(QString station, exportStations) {
         ExportStations.insert(station);
     }
 }



