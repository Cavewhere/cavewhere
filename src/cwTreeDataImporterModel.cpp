/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTreeDataImporterModel.h"
#include "cwTreeImportData.h"
#include "cwTreeImportDataNode.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwGlobalIcons.h"

//Qt includes
#include <QPixmapCache>
#include <QDebug>

cwTreeDataImporterModel::cwTreeDataImporterModel(QObject *parent) :
    QAbstractItemModel(parent),
    GlobalData(nullptr)
{
}

/**
  \brief Get's the number of columns of the model
  Always 1
  */
int cwTreeDataImporterModel::columnCount ( const QModelIndex & /*parent*/) const {
    if(GlobalData == nullptr) { return 0; }
    return NumberOfColumns;
}

/**
   \brief Get's the data at index
   */
QVariant cwTreeDataImporterModel::data ( const QModelIndex & index, int role) const {
    if(GlobalData == nullptr) { return QVariant(); }
    if(!index.isValid()) { return QVariant(); }

    return NameColumnData(index, role);
}

/**
   \brief Get's the name's column data
   */
QVariant cwTreeDataImporterModel::NameColumnData(const QModelIndex & index, int role) const {

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
QVariant cwTreeDataImporterModel::NameColumnDisplayData(const QModelIndex& index) const {
    //This index is a shot
    if(isShot(index)) {

//        Q_ASSERT(dynamic_cast<cwTreeImportDataNode*>(index.internalPointer()) != nullptr);
        cwTreeImportDataNode* parentNode = static_cast<cwTreeImportDataNode*>(index.internalPointer());
        int shotIndex = index.row() - parentNode->childNodeCount();

        cwSurveyChunk* parentSurveyChunk = parentNode->parentChunkOfShot(shotIndex);
        int localChunkShotIndex = parentNode->chunkShotIndex(shotIndex);

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

    //This index is a node
    cwTreeImportDataNode* node = toNode(index);
    if(node != nullptr) {
        if(node->name().isEmpty()) {
            return QVariant("Untitled");
        }

        return QVariant(node->name());
    }

    return QVariant();
}

/**
   \brief Get's the icon data for the item
   */
QVariant cwTreeDataImporterModel::NameColumnIconData(const QModelIndex& index) const {
    //This index is a node
    cwTreeImportDataNode* node = toNode(index);
    if(node != nullptr) {
        QPixmap icon;

        switch(node->importType()) {
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
QModelIndex cwTreeDataImporterModel::index ( int row, int /*column*/, const QModelIndex & parent) const {
    if(GlobalData == nullptr) { return QModelIndex(); }

    if(!parent.isValid()) {
        //This is the root use the root data
        QList<cwTreeImportDataNode*> rootNodes = GlobalData->nodes();
        if(row >= rootNodes.size()) {
            return QModelIndex(); //Invalid index
        }

        return createIndex(row, 0, (void*)nullptr);
    }

    //This if the index
    cwTreeImportDataNode* parentNode = toNode(parent);
    if(parentNode != nullptr) {
        return createIndex(row, 0, parentNode);
    }

    return QModelIndex();
}

/**
   \brief Gets the parent index of index
   */
QModelIndex cwTreeDataImporterModel::parent ( const QModelIndex & index ) const {
    if(GlobalData == nullptr) { return QModelIndex(); }

    if(!index.isValid()) { return QModelIndex(); }

    if(index.internalPointer() == nullptr) { return QModelIndex(); }

    //The grandparent's node
    cwTreeImportDataNode* parentNode = static_cast<cwTreeImportDataNode*>(index.internalPointer());
    cwTreeImportDataNode* grandparentNode = parentNode->parentNode();
    if(grandparentNode == nullptr) {
        //In the GlobalData
        int row = GlobalData->nodes().indexOf(parentNode);
        return createIndex(row, 0, (void*)nullptr);
    } else {
        int row = grandparentNode->childNodes().indexOf(parentNode);
        return createIndex(row, 0, grandparentNode);
    }
}

/**
   \brief Gets the number of rows
   */
int cwTreeDataImporterModel::rowCount ( const QModelIndex & parent ) const {
    if(GlobalData == nullptr) { return 0; }
    if(parent == QModelIndex()) { return GlobalData->nodes().count(); }

    cwTreeImportDataNode* node = toNode(parent);
    if(node != nullptr) {
        return node->childNodeCount() + node->shotCount();
    }

    return 0;
}

/**
  \brief Sets the data for the model
  */
void cwTreeDataImporterModel::setTreeImportData(cwTreeImportData* data) {
    beginResetModel();
    GlobalData = data;
    endResetModel();

    if(GlobalData == nullptr) { return; }

    //Go through the GlobalData and hook it up to this object
    foreach(cwTreeImportDataNode* node, GlobalData->nodes()) {
        connectNode(node);
    }
}

/**
  \brief A recusive function that connect node and it's sub nodes and shots to this model
  */
void cwTreeDataImporterModel::connectNode(cwTreeImportDataNode* node) {
    connect(node, SIGNAL(nameChanged()), SLOT(nodeDataChanged()));
    connect(node, SIGNAL(importTypeChanged()), SLOT(nodeDataChanged()));

    foreach(cwTreeImportDataNode* childNode, node->childNodes()) {
        connectNode(childNode);
    }
}

/**
  Test to see if index is a node
  */
bool cwTreeDataImporterModel::isNode(const QModelIndex &index) const {
    if(index.internalPointer() == nullptr) {
        return true;
    } else {
        cwTreeImportDataNode* node = static_cast<cwTreeImportDataNode*>(index.internalPointer());
        return index.row() < node->childNodeCount();
    }
}

/**
  Test to see if index is a shot
  */
bool cwTreeDataImporterModel::isShot(const QModelIndex &index) const
{
    return !isNode(index);
}

/**
  \brief Converts the index into a cwTreeImportDataNode

  If the index is invalid or it's not node data, this returns nullptr

  The point that's coming out of this function should not be delete or stored
  */
cwTreeImportDataNode* cwTreeDataImporterModel::toNode(const QModelIndex& index) const {
    if(!index.isValid()) { return nullptr; }

    if(isNode(index)) {
        cwTreeImportDataNode* node = static_cast<cwTreeImportDataNode*>(index.internalPointer());
        if(node != nullptr) {
            return node->childNode(index.row());
        } else {
            return GlobalData->nodes()[index.row()];
        }
    }

    //This is a shot
    return nullptr;
}

/**
  \brief Converts a node to an index

  If the node isn't part of the model, then this returns a invalid index
  */
QModelIndex cwTreeDataImporterModel::toIndex(cwTreeImportDataNode* node) {
    if(node == nullptr) { return QModelIndex(); }

    //This is a root node
    int row;
    cwTreeImportDataNode* parentNode = nullptr;
    if(node->parentNode() == nullptr) {
        row = GlobalData->nodes().indexOf(node);
    } else {
        //This is some other leaf node
        parentNode = node->parentNode();
        row = parentNode->childNodes().indexOf(node);
    }

    //If the row is invalid
    if(row < 0) {
        return QModelIndex();
    }

    //Node is valid, create the index
    return createIndex(row, 0, parentNode);
}

/**
  \brief Called when a node's data has changed
  */
void cwTreeDataImporterModel::nodeDataChanged() {
    Q_ASSERT(qobject_cast<cwTreeImportDataNode*>(sender()) != nullptr);
    cwTreeImportDataNode* node = static_cast<cwTreeImportDataNode*>(sender());
    QModelIndex nodeIndex = toIndex(node);
    if(!nodeIndex.isValid()) { return; }
    emit dataChanged(nodeIndex, nodeIndex);
}
