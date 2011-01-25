//Our includes
#include "cwSurvexImporterModel.h"
#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwGlobalIcons.h"

//Qt includes
#include <QPixmapCache>
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
     return NumberOfColumns;
 }

 /**
   \brief Get's the data at index
   */
 QVariant cwSurvexImporterModel::data ( const QModelIndex & index, int role) const {
     if(GlobalData == NULL) { return QVariant(); }
     if(!index.isValid()) { return QVariant(); }

     return NameColumnData(index, role);
 }

 /**
   \brief Get's the name's column data
   */
 QVariant cwSurvexImporterModel::NameColumnData(const QModelIndex & index, int role) const {

     switch(role) {
     case Qt::DisplayRole:
         return NameColumnDisplayData(index);
     case Qt::DecorationRole:
         return NameColumnIconData(index);
     default:
         return QVariant();
     }

     return QVariant();
 }

 /**
   \brief Gets the dislay data for the index

   This return the name of the index
   */
 QVariant cwSurvexImporterModel::NameColumnDisplayData(const QModelIndex& index) const {
     //This index is a shot
     cwShot* shot = toShot(index); //qobject_cast<cwShot*>(static_cast<QObject*>(index.internalPointer()));
     if(shot != NULL) {
         cwStation* fromStation = shot->fromStation();
         cwStation* toStation = shot->toStation();

         QString fromStationName;
         QString toStationName;
         QString errorStationName = "(Error - Unknown staton name)";

         if(fromStation == NULL) {
             fromStationName = errorStationName;
         } else {
             fromStationName = fromStation->name();
         }

         if(toStation == NULL) {
             toStationName = errorStationName;
         } else {
             toStationName = toStation->name();
         }

         QString displayText = QString("%1 %2 %3").arg(fromStationName).arg(QString(QChar(0x2192))).arg(toStationName);

         return QVariant(displayText);
     }

     //This index is a block
     cwSurvexBlockData* block = toBlockData(index); //qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(index.internalPointer()));
     if(block != NULL) {
         if(block->name().isEmpty()) {
             return QVariant("Untitled");
         }

         return QVariant(block->name());
     }

     return QVariant();
 }

 /**
   \brief Get's the icon data for the item
   */
 QVariant cwSurvexImporterModel::NameColumnIconData(const QModelIndex& index) const {
     //This index is a block
     cwSurvexBlockData* block = toBlockData(index); //qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(index.internalPointer()));
     if(block != NULL) {
         QPixmap icon;

         switch(block->importType()) {
         case cwSurvexBlockData::NoImport:
             if(!QPixmapCache::find(cwGlobalIcons::NoImport, &icon)) {
                 icon = QPixmap(cwGlobalIcons::NoImportFilename);
                 cwGlobalIcons::NoImport = QPixmapCache::insert(icon);
             }
             break;
         case cwSurvexBlockData::Trip:
             if(!QPixmapCache::find(cwGlobalIcons::Trip, &icon)) {
                 icon = QPixmap(cwGlobalIcons::TripFilename);
                 cwGlobalIcons::Trip = QPixmapCache::insert(icon);
             }
             break;
         case cwSurvexBlockData::Cave:
             if(!QPixmapCache::find(cwGlobalIcons::Cave, &icon)) {
                 icon = QPixmap(cwGlobalIcons::CaveFilename);
                 cwGlobalIcons::Cave = QPixmapCache::insert(icon);
             }
             break;
         case cwSurvexBlockData::Structure:
             if(!QPixmapCache::find(cwGlobalIcons::Plus, &icon)) {
                 icon = QPixmap(cwGlobalIcons::PlusFilename);
                 cwGlobalIcons::Plus = QPixmapCache::insert(icon);
             }
         }

         return QVariant(icon);
     }
     return QVariant();
 }

 /**
   \brief Gets the index at row and column of parent
   */
 QModelIndex cwSurvexImporterModel::index ( int row, int column, const QModelIndex & parent) const {
     if(GlobalData == NULL) { return QModelIndex(); }

     if(!parent.isValid()) {
         //This is the root use the root data
         QList<cwSurvexBlockData*> rootBlocks = GlobalData->blocks();
         if(row >= rootBlocks.size()) {
             return QModelIndex(); //Invalid index
         }

         cwSurvexBlockData* block = rootBlocks[row];
         return createIndex(row, column, block);
     }

     //This if the index
     cwSurvexBlockData* block = toBlockData(parent); //qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(parent.internalPointer()));
     if(block != NULL) {
         if(row < block->childBlockCount()) {
             //This is another block index
             cwSurvexBlockData* childBlock = block->childBlock(row);
             return createIndex(row, column, childBlock);
         }

         int blockIndex = row - block->childBlockCount();
         if(blockIndex < block->shotCount()) {
             //This is a shot
             cwShot* shot = block->shot(blockIndex);
             return createIndex(row, column, shot);
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

     cwSurvexBlockData* block = toBlockData(index); //qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(index.internalPointer()));
     cwSurvexBlockData* parentBlock = NULL;
     if(block != NULL) {
         parentBlock = block->parentBlock();
     }

     cwShot* shot = toShot(index);
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
         return createIndex(row, index.column(), parentBlock);
     } else {
         int row = grandparentBlock->childBlocks().indexOf(parentBlock);
         return createIndex(row, index.column(), parentBlock);
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

     cwSurvexBlockData* block = toBlockData(parent);
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

    if(GlobalData == NULL) { return; }

    //Go through the GlobalData and hook it up to this object
    foreach(cwSurvexBlockData* block, GlobalData->blocks()) {
        connectBlock(block);
    }
}

/**
  \brief A recusive function that connect block and it's sub blocks and shots to this model
  */
void cwSurvexImporterModel::connectBlock(cwSurvexBlockData* block) {
    connect(block, SIGNAL(nameChanged()), SLOT(blockDataChanged()));
    connect(block, SIGNAL(importTypeChanged()), SLOT(blockDataChanged()));

    foreach(cwSurvexBlockData* childBlock, block->childBlocks()) {
        connectBlock(childBlock);
    }

//    for(int i = 0; i < block->shotCount(); i++) {
//        cwShot* shot = block->shot(i);
//        cwStation* from = shot->fromStation();
//        cwStation* to = shot->toStation();

//        connect(from, SIGNAL())
//    }
}

/**
  \brief Converts the index into a cwSurvexBlockData

  If the index is invalid or it's not block data, this returns NULL

  The point that's coming out of this function should not be delete or stored
  */
cwSurvexBlockData* cwSurvexImporterModel::toBlockData(const QModelIndex& index) const {
    if(!index.isValid()) { return NULL; }
    return qobject_cast<cwSurvexBlockData*>(static_cast<QObject*>(index.internalPointer()));
}


/**
  \brief Converts the index into a cwSurvexBlockData

  If the index is invalid or it's not block data, this returns NULL

  The point that's coming out of this function should not be delete or stored
  */
cwShot* cwSurvexImporterModel::toShot(const QModelIndex& index) const {
    if(!index.isValid()) { return NULL; }
    return qobject_cast<cwShot*>(static_cast<QObject*>(index.internalPointer()));
}

/**
  \brief Converts a block to an index

  If the block isn't part of the model, then this returns a invalid index
  */
QModelIndex cwSurvexImporterModel::toIndex(cwSurvexBlockData* block) {
    if(block == NULL) { return QModelIndex(); }

    //This is a root block
    int row;
    if(block->parentBlock() == NULL) {
        row = GlobalData->blocks().indexOf(block);
    } else {
        //This is some other leaf block
        cwSurvexBlockData* parentBlock = block->parentBlock();
        row = parentBlock->childBlocks().indexOf(block);
    }

    //If the row is invalid
    if(row < 0) {
        return QModelIndex();
    }

    //Block is valid, create the index
    return createIndex(row, 0, block);
}

/**
  \brief Converts a shot to an index

  If the shot isn't part of the model, then this returns an invalid index
  */
QModelIndex cwSurvexImporterModel::toIndex(cwShot* shot) {
    if(shot == NULL) { return QModelIndex(); }

    //Make the shot is part of a chunk
    cwSurveyChunk* parentChunk = shot->parentChunk();
    if(parentChunk == NULL) { return QModelIndex(); }

    //Make sure that the parentChunk is cwSurvexBlockData
    cwSurvexBlockData* parentBlockData = qobject_cast<cwSurvexBlockData*>(parentChunk->parent());
    QModelIndex indexParentBlockData = toIndex(parentBlockData);
    if(indexParentBlockData.isValid()) { return QModelIndex(); }

    //Find the shot in the chunk
    int shotIndex = parentBlockData->indexOfShot(shot);
    if(shotIndex < 0) { return QModelIndex(); } //Couldn't find shot in the block

    //Found the block
    int row = parentBlockData->childBlockCount() + shotIndex;
    return createIndex(row, 0, shot);
}

/**
  \brief Called when a block's data has changed
  */
void cwSurvexImporterModel::blockDataChanged() {
    cwSurvexBlockData* block = static_cast<cwSurvexBlockData*>(sender());
    QModelIndex blockIndex = toIndex(block);
    if(!blockIndex.isValid()) { return; }
    emit dataChanged(blockIndex, blockIndex);
}

/**
  \brief Called when a shot's data has changed
  */
void cwSurvexImporterModel::shotDataChanged() {
    cwShot* shot = static_cast<cwShot*>(sender());
    QModelIndex shotIndex = toIndex(shot);
    if(!shotIndex.isValid()) { return; }
    emit dataChanged(shotIndex, shotIndex);
}
