//Our includes
#include "cwSurvexImporterModel.h"
#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwStation.h"

//Qt includes
#include <QDebug>

cwSurvexImporterModel::cwSurvexImporterModel(QObject *parent) :
    QAbstractItemModel(parent),
    GlobalData(NULL)
{
}

/**
  \brief Get's the number of columns of the model
  Always 1
  */
 int cwSurvexImporterModel::columnCount ( const QModelIndex & /*parent*/) const {
     if(GlobalData == NULL) { return 0; }
     return 1;
 }

 /**
   \brief Get's the data at index
   */
 QVariant cwSurvexImporterModel::data ( const QModelIndex & index, int role) const {
     if(GlobalData == NULL) { return QVariant(); }
     if(!index.isValid()) { return QVariant(); }

     if(role != Qt::DisplayRole) { return QVariant(); }

     //This index is a shot
     cwShot* shot = qobject_cast<cwShot*>(static_cast<QObject*>(index.internalPointer()));
     if(shot != NULL) {
         cwStation* fromStation = shot->fromStation();
         cwStation* toStation = shot->toStation();

         QString fromStationName;
         QString toStationName;
         QString errorStationName = "(Error - Unknown staton name)";

         if(fromStation == NULL) {
             fromStationName = errorStationName;
         } else {
             fromStationName = fromStation->GetName();
         }

         if(toStation == NULL) {
             toStationName = errorStationName;
         } else {
             toStationName = toStation->GetName();
         }

         QString displayText = QString("%1 %2 %3").arg(fromStationName).arg(QString(QChar(0x2192))).arg(toStationName);

         return QVariant(displayText);
     }

     //This index is a block
     cwSurvexBlockData* block = qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(index.internalPointer()));
     if(block != NULL) {
         if(block->name().isEmpty()) {
             return QVariant("Untitled *begin and *end");
         }

         return QVariant(block->name());
     }

     return QVariant();
 }

 /**
   \brief Gets the index at row and column of parent
   */
 QModelIndex cwSurvexImporterModel::index ( int row, int /*column*/, const QModelIndex & parent) const {
     if(GlobalData == NULL) { return QModelIndex(); }

     if(!parent.isValid()) {
         //This is the root use the root data
         QList<cwSurvexBlockData*> rootBlocks = GlobalData->blocks();
         if(row >= rootBlocks.size()) {
             return QModelIndex(); //Invalid index
         }

         cwSurvexBlockData* block = rootBlocks[row];
         return createIndex(row, 0, block);
     }

     //This if the index
     cwSurvexBlockData* block = qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(parent.internalPointer()));
     if(block != NULL) {
         if(row < block->childBlockCount()) {
             //This is another block index
             cwSurvexBlockData* childBlock = block->childBlock(row);
             return createIndex(row, 0, childBlock);
         }

         int blockIndex = row - block->childBlockCount();
         if(blockIndex < block->shotCount()) {
             //This is a shot
             cwShot* shot = block->shot(blockIndex);
             return createIndex(row, 0, shot);
         }
     }

     return QModelIndex();
 }

 /**
   \brief Gets the parent index of index
   */
 QModelIndex cwSurvexImporterModel::parent ( const QModelIndex & index ) const {
     if(GlobalData == NULL) { return QModelIndex(); }

     if(!index.isValid()) { return QModelIndex(); }

     cwSurvexBlockData* block = qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(index.internalPointer()));
     cwSurvexBlockData* parentBlock = NULL;
     if(block != NULL) {
         parentBlock = block->parentBlock();
     }

     cwShot* shot = qobject_cast<cwShot*>(static_cast<QObject*>(index.internalPointer()));
     if(shot != NULL) {
         QObject* shotParent = shot->parent(); //should be a chunk
         if(shotParent != NULL) {
             parentBlock = qobject_cast<cwSurvexBlockData*>(shotParent->parent()); //Should be a block
         }
     }

     if(parentBlock == NULL) {
         //Index is the root
         return QModelIndex();
     }
     //The grandparent's block
     cwSurvexBlockData* grandparentBlock = parentBlock->parentBlock();
     if(grandparentBlock == NULL) {
         //In the GlobalData
         int row = GlobalData->blocks().indexOf(parentBlock);
         return createIndex(row, 0, parentBlock);
     } else {
         int row = grandparentBlock->childBlocks().indexOf(parentBlock);
         return createIndex(row, 0, parentBlock);
     }

     return QModelIndex(); //Invalid object, so invalid parent
 }

 /**
   \brief Gets the number of rows
   */
 int cwSurvexImporterModel::rowCount ( const QModelIndex & parent ) const {
     if(GlobalData == NULL) { return 0; }

     if(!parent.isValid()) {
         return GlobalData->blocks().count();
     }

     cwSurvexBlockData* block = qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(parent.internalPointer()));
     if(block != NULL) {
         return block->childBlockCount() + block->shotCount();
     }

     return 0;
 }

/**
  \brief Sets the data for the model
  */
void cwSurvexImporterModel::setSurvexData(cwSurvexGlobalData* data) {
    beginResetModel();
    GlobalData = data;
    endResetModel();
}


