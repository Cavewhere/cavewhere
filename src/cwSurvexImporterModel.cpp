/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexImporterModel.h"
#include "cwTreeImportData.h"
#include "cwTreeImportDataNode.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwGlobalIcons.h"

//Qt includes
#include <QPixmapCache>
#include <QDebug>

cwSurvexImporterModel::cwSurvexImporterModel(QObject *parent) :
    QAbstractItemModel(parent),
    GlobalData(nullptr)
{
}

/**
  \brief Get's the number of columns of the model
  Always 1
  */
int cwSurvexImporterModel::columnCount ( const QModelIndex & /*parent*/) const {
    if(GlobalData == nullptr) { return 0; }
    return NumberOfColumns;
}

/**
   \brief Get's the data at index
   */
QVariant cwSurvexImporterModel::data ( const QModelIndex & index, int role) const {
    if(GlobalData == nullptr) { return QVariant(); }
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
    if(isShot(index)) {

//        Q_ASSERT(dynamic_cast<cwSurvexBlockData*>(index.internalPointer()) != nullptr);
        cwTreeImportDataNode* parentBlock = static_cast<cwTreeImportDataNode*>(index.internalPointer());
        int shotIndex = index.row() - parentBlock->childBlockCount();

        cwSurveyChunk* parentSurveyChunk = parentBlock->parentChunkOfShot(shotIndex);
        int localChunkShotIndex = parentBlock->chunkShotIndex(shotIndex);

        cwStation fromStation = parentSurveyChunk->station(localChunkShotIndex);
        cwStation toStation = parentSurveyChunk->station(localChunkShotIndex + 1);

        QString fromStationName;
        QString toStationName;
        QString errorStationName = "(Error - Unknown staton name)";

        if(!fromStation.isValid()) {
            fromStationName = errorStationName;
        } else {
            fromStationName = fromStation.name();
        }

        if(!toStation.isValid()) {
            toStationName = errorStationName;
        } else {
            toStationName = toStation.name();
        }

        QString displayText = QString("%1 %2 %3").arg(fromStationName).arg(QString(QChar(0x2192))).arg(toStationName);

        return QVariant(displayText);
    }

    //This index is a block
    cwTreeImportDataNode* block = toBlockData(index);
    if(block != nullptr) {
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
    cwTreeImportDataNode* block = toBlockData(index);
    if(block != nullptr) {
        QPixmap icon;

        switch(block->importType()) {
        case cwTreeImportDataNode::NoImport:
            if(!QPixmapCache::find(cwGlobalIcons::NoImport, &icon)) {
                icon = QPixmap(cwGlobalIcons::NoImportFilename);
                cwGlobalIcons::NoImport = QPixmapCache::insert(icon);
            }
            break;
        case cwTreeImportDataNode::Trip:
            if(!QPixmapCache::find(cwGlobalIcons::Trip, &icon)) {
                icon = QPixmap(cwGlobalIcons::TripFilename);
                cwGlobalIcons::Trip = QPixmapCache::insert(icon);
            }
            break;
        case cwTreeImportDataNode::Cave:
            if(!QPixmapCache::find(cwGlobalIcons::Cave, &icon)) {
                icon = QPixmap(cwGlobalIcons::CaveFilename);
                cwGlobalIcons::Cave = QPixmapCache::insert(icon);
            }
            break;
        case cwTreeImportDataNode::Structure:
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
QModelIndex cwSurvexImporterModel::index ( int row, int /*column*/, const QModelIndex & parent) const {
    if(GlobalData == nullptr) { return QModelIndex(); }

    if(!parent.isValid()) {
        //This is the root use the root data
        QList<cwTreeImportDataNode*> rootBlocks = GlobalData->blocks();
        if(row >= rootBlocks.size()) {
            return QModelIndex(); //Invalid index
        }

        return createIndex(row, 0, (void*)nullptr);
    }

    //This if the index
    cwTreeImportDataNode* parentBlock = toBlockData(parent);
    if(parentBlock != nullptr) {
        return createIndex(row, 0, parentBlock);
    }

    return QModelIndex();
}

/**
   \brief Gets the parent index of index
   */
QModelIndex cwSurvexImporterModel::parent ( const QModelIndex & index ) const {
    if(GlobalData == nullptr) { return QModelIndex(); }

    if(!index.isValid()) { return QModelIndex(); }

    if(index.internalPointer() == nullptr) { return QModelIndex(); }

    //The grandparent's block
    cwTreeImportDataNode* parentBlock = static_cast<cwTreeImportDataNode*>(index.internalPointer());
    cwTreeImportDataNode* grandparentBlock = parentBlock->parentBlock();
    if(grandparentBlock == nullptr) {
        //In the GlobalData
        int row = GlobalData->blocks().indexOf(parentBlock);
        return createIndex(row, 0, (void*)nullptr);
    } else {
        int row = grandparentBlock->childBlocks().indexOf(parentBlock);
        return createIndex(row, 0, grandparentBlock);
    }
}

/**
   \brief Gets the number of rows
   */
int cwSurvexImporterModel::rowCount ( const QModelIndex & parent ) const {
    if(GlobalData == nullptr) { return 0; }
    if(parent == QModelIndex()) { return GlobalData->blocks().count(); }

    cwTreeImportDataNode* block = toBlockData(parent);
    if(block != nullptr) {
        return block->childBlockCount() + block->shotCount();
    }

    return 0;
}

/**
  \brief Sets the data for the model
  */
void cwSurvexImporterModel::setSurvexData(cwTreeImportData* data) {
    beginResetModel();
    GlobalData = data;
    endResetModel();

    if(GlobalData == nullptr) { return; }

    //Go through the GlobalData and hook it up to this object
    foreach(cwTreeImportDataNode* block, GlobalData->blocks()) {
        connectBlock(block);
    }
}

/**
  \brief A recusive function that connect block and it's sub blocks and shots to this model
  */
void cwSurvexImporterModel::connectBlock(cwTreeImportDataNode* block) {
    connect(block, SIGNAL(nameChanged()), SLOT(blockDataChanged()));
    connect(block, SIGNAL(importTypeChanged()), SLOT(blockDataChanged()));

    foreach(cwTreeImportDataNode* childBlock, block->childBlocks()) {
        connectBlock(childBlock);
    }
}

/**
  Test to see if index is a block
  */
bool cwSurvexImporterModel::isBlock(const QModelIndex &index) const {
    if(index.internalPointer() == nullptr) {
        return true;
    } else {
        cwTreeImportDataNode* block = static_cast<cwTreeImportDataNode*>(index.internalPointer());
        return index.row() < block->childBlockCount();
    }
}

/**
  Test to see if index is a shot
  */
bool cwSurvexImporterModel::isShot(const QModelIndex &index) const
{
    return !isBlock(index);
}

/**
  \brief Converts the index into a cwSurvexBlockData

  If the index is invalid or it's not block data, this returns nullptr

  The point that's coming out of this function should not be delete or stored
  */
cwTreeImportDataNode* cwSurvexImporterModel::toBlockData(const QModelIndex& index) const {
    if(!index.isValid()) { return nullptr; }

    if(isBlock(index)) {
        cwTreeImportDataNode* block = static_cast<cwTreeImportDataNode*>(index.internalPointer());
        if(block != nullptr) {
            return block->childBlock(index.row());
        } else {
            return GlobalData->blocks()[index.row()];
        }
    }

    //This is a shot
    return nullptr;
}

/**
  \brief Converts a block to an index

  If the block isn't part of the model, then this returns a invalid index
  */
QModelIndex cwSurvexImporterModel::toIndex(cwTreeImportDataNode* block) {
    if(block == nullptr) { return QModelIndex(); }

    //This is a root block
    int row;
    cwTreeImportDataNode* parentBlock = nullptr;
    if(block->parentBlock() == nullptr) {
        row = GlobalData->blocks().indexOf(block);
    } else {
        //This is some other leaf block
        parentBlock = block->parentBlock();
        row = parentBlock->childBlocks().indexOf(block);
    }

    //If the row is invalid
    if(row < 0) {
        return QModelIndex();
    }

    //Block is valid, create the index
    return createIndex(row, 0, parentBlock);
}

/**
  \brief Called when a block's data has changed
  */
void cwSurvexImporterModel::blockDataChanged() {
    Q_ASSERT(qobject_cast<cwTreeImportDataNode*>(sender()) != nullptr);
    cwTreeImportDataNode* block = static_cast<cwTreeImportDataNode*>(sender());
    QModelIndex blockIndex = toIndex(block);
    if(!blockIndex.isValid()) { return; }
    emit dataChanged(blockIndex, blockIndex);
}
