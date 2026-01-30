/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwDebug.h"
#include "cwProject.h"
#include "cwData.h"

//Qt includes
#include <QThread>
#include <QDebug>

cwCavingRegion::cwCavingRegion(QObject *parent) :
    QAbstractListModel(parent)
{
}

// /**
//   \brief Copy constructor
//   */
// cwCavingRegion::cwCavingRegion(const cwCavingRegion& object) :
//     QAbstractListModel(nullptr),
//     cwUndoer(object.undoStack())
// {
//     copy(object);
// }

// /**
//   \brief Alignment operator
//   */
// cwCavingRegion& cwCavingRegion::operator=(const cwCavingRegion& object) {
//     return copy(object);
// }

/**
 * @brief cwCavingRegion::rowCount
 * @param parent
 * @return
 */
int cwCavingRegion::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return caveCount();
}

/**
 * @brief cwCavingRegion::data
 * @param index
 * @param role
 * @return
 */
QVariant cwCavingRegion::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    switch(role) {
    case CaveObjectRole:
        return QVariant::fromValue(m_caves.at(index.row()));
    }

    return QVariant();
}

/**
 * @brief cwCavingRegion::roleNames
 * @return
 */
QHash<int, QByteArray> cwCavingRegion::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(CaveObjectRole, "caveObjectRole");
    return roles;
}

/**
 * @brief cwCavingRegion::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwCavingRegion::index(int row, int column, const QModelIndex &parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

// /**
//   \brief Copy's the object into this object
//   */
// cwCavingRegion& cwCavingRegion::copy(const cwCavingRegion& object) {
//     Q_ASSERT(object.thread() == thread() || object.thread() == nullptr || thread() == nullptr);
//     Q_ASSERT(QThread::currentThread() == thread() || thread() == nullptr);

//     if(&object == this) {
//         return *this;
//     }

//     //Clear old caves
//     int lastIndex = m_caves.size() - 1;
//     removeCaves(0, lastIndex);

//     if(!object.m_caves.isEmpty()) {
//         emit beginInsertCaves(0, object.m_caves.size() - 1);
//         emit beginInsertRows(QModelIndex(), 0, object.m_caves.size() - 1);
//     }

//     //Add new caves
//     m_caves.reserve(object.m_caves.size());
//     foreach(cwCave* cave, object.m_caves) {

//         //Strange copying to make sure the newCaves are
//         //On the correct thread
//         bool threadIsNull = thread() == nullptr;
//         if(threadIsNull) {
//             moveToThread(QThread::currentThread());
//         }

//         cwCave* newCave = new cwCave(*cave);
//         newCave->setParent(this);  //Uncomment because this cause problems with QML

//         if(threadIsNull) {
//             moveToThread(nullptr);
//         }

//         m_caves.append(newCave);
//     }

//     if(m_caves.size() - 1 >= 0) {
//         emit insertedCaves(0, m_caves.size() -1);
//         emit endInsertRows();
//         emit caveCountChanged();
//     }

//     return *this;
// }


/**
  \brief Creates a new cave and adds it to the caving region
  */
void cwCavingRegion::addCaveHelper() {
    QString newCaveName = QString("Cave %1").arg(caveCount() + 1);
    beginUndoMacro(QString("Add %1").arg(newCaveName));

    cwCave* cave = new cwCave();
    cave->setUndoStack(undoStack());
    cave->setName(newCaveName);
    addCave(cave);

    endUndoMacro();
}

/**
  \brief Adds a cave to the region
  */
void cwCavingRegion::addCave(cwCave* cave) {
    if(cave == nullptr) {
        addCaveHelper();
        return;
    };
    insertCave(m_caves.size(), cave);
}

void cwCavingRegion::addCaves(QList<cwCave*> caves) {
    foreach(cwCave* cave, caves) {
        unparentCave(cave);
        cave->setUndoStack(undoStack());
    }

    //Run the insert cave command
    if(!caves.isEmpty()) {
        int firstIndex = m_caves.size();
        pushUndo(new InsertCaveCommand(this, caves, firstIndex));
    }
}

/**
  \brief Inserts a cave into the region at inedx
  */
void cwCavingRegion::insertCave(int index, cwCave* cave) {
    if(index < 0 || index > m_caves.size()) { return; }

    unparentCave(cave);

    //Run the insert cave command
    pushUndo(new InsertCaveCommand(this, cave, index));
}

/**
  \brief Removes the cave at index
  */
void cwCavingRegion::removeCave(int index) {
    removeCaves(index, index);
}

/**
  \brief Remove all the caves between beginIndex and the endIndex

  The caves will be delete at a later time
  */
void cwCavingRegion::removeCaves(int beginIndex, int endIndex) {
    //Make sure the indexes are good
    if(beginIndex < 0 || beginIndex >= m_caves.size() ||
            endIndex < 0 || endIndex >= m_caves.size()) {
        return;
    }

    //The beginIndex needs to be greater than the end index
    if(beginIndex > endIndex) {
        return;
    }

    pushUndo(new RemoveCaveCommand(this, beginIndex, endIndex));
}

/**
  \brief Removes all the caves from the region
  */
void cwCavingRegion::clearCaves() {
    if(!m_caves.isEmpty()) {
        removeCaves(0, m_caves.size() - 1);
    }
}

/**
  \brief Get's the index of the cave
  */
int cwCavingRegion::indexOf(cwCave* cave) {
    return m_caves.indexOf(cave);
}

cwProject *cwCavingRegion::parentProject() const
{
    return dynamic_cast<cwProject*>(parent());
}

void cwCavingRegion::setData(const cwCavingRegionData &data)
{
    setName(data.name);

    clearCaves();

    QList<cwCave*> newCaves;
    newCaves.reserve(data.caves.size());
    for(const auto& caveData : data.caves) {
        auto newCave = new cwCave(this);
        newCave->setData(caveData);
        newCaves.append(newCave);
    }
    addCaves(newCaves);
}

cwCavingRegionData cwCavingRegion::data() const
{    
    return {
        m_name.value(),
        cwData::toDataList<cwCaveData>(m_caves)
    };
}

/**
  \brief Sets the undo stack for this region

  This will also set undo stack for the children as well
  */
void cwCavingRegion::setUndoStackForChildren() {
    setUndoStackForChildrenHelper(m_caves);
}


/**
  \brief Unparents the cave
  */
void cwCavingRegion::unparentCave(cwCave* cave) {
    //Reparent the trip, if already in another cave
    cwCavingRegion* parentRegion = dynamic_cast<cwCavingRegion*>(((QObject*)cave)->parent());
    if(parentRegion != nullptr) {
        int index = parentRegion->m_caves.indexOf(cave);
        parentRegion->removeCave(index);
    }
}

cwCavingRegion::InsertRemoveCave::InsertRemoveCave(cwCavingRegion* region,
                                                   int beginIndex, int endIndex) {
    Region = region;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
    OwnsCaves = false;
}

/**
  Delete all the caves if it owns them
  */
cwCavingRegion::InsertRemoveCave::~InsertRemoveCave() {
    if(OwnsCaves) {
        foreach(auto cave, Caves) {
            if(cave) {
                cave->deleteLater();
            }
        }
    }
}

/**
  \brief Insert the caves in this command into the region
  */
void cwCavingRegion::InsertRemoveCave::insertCaves() {
   // if(Region.isNull()) { return; }
    cwCavingRegion* regionPtr = Region; //.data();

    emit regionPtr->beginInsertCaves(BeginIndex, EndIndex);
    emit regionPtr->beginInsertRows(QModelIndex(), BeginIndex, EndIndex);
    for(int i = 0; i < Caves.size(); i++) {
        int index = BeginIndex + i;
        regionPtr->m_caves.insert(index, Caves[i]);
        Caves[i]->setParent(regionPtr);
    }

    OwnsCaves = false;

    emit regionPtr->insertedCaves(BeginIndex, EndIndex);
    emit regionPtr->endInsertRows();
    emit regionPtr->caveCountChanged();
}

/**
  \brief Removes the caves in this command from the region
  */
void cwCavingRegion::InsertRemoveCave::removeCaves() {
//    if(Region.isNull()) { return; }
    cwCavingRegion* regionPtr = Region; //.data();

    emit regionPtr->beginRemoveCaves(BeginIndex, EndIndex);
    emit regionPtr->beginRemoveRows(QModelIndex(), BeginIndex, EndIndex);

    for(int i = Caves.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        regionPtr->m_caves.removeAt(index);

        //Do NOT uncomment, qml engine may garbage collect objects that aren't parented, and can cause double free problem
        // Caves[i]->setParent(nullptr);
    }

    OwnsCaves = true;

    emit regionPtr->removedCaves(BeginIndex, EndIndex);
    emit regionPtr->endRemoveRows();
    emit regionPtr->caveCountChanged();
}


cwCavingRegion::InsertCaveCommand::InsertCaveCommand(cwCavingRegion* parentRegion,
                                                     QList<cwCave*> caves,
                                                     int index) :
    cwCavingRegion::InsertRemoveCave(parentRegion, index, index + caves.size() -1)
{
    Caves.reserve(caves.size());
    for(auto cave : caves) {
        Caves.append(cave);
    }

    if(caves.size() == 1) {
        setText(QString("Add %1").arg(caves.first()->name()));
    } else {
        setText(QString("Add %1 caves").arg(caves.size()));
    }
}

cwCavingRegion::InsertCaveCommand::InsertCaveCommand(cwCavingRegion* parentRegion,
                                                     cwCave* cave,
                                                     int index) :
    cwCavingRegion::InsertRemoveCave(parentRegion, index, index)
{
    Caves.append(cave);
    setText(QString("Add %1").arg(cave->name()));
}

void cwCavingRegion::InsertCaveCommand::redo() {
    insertCaves();
}

void cwCavingRegion::InsertCaveCommand::undo() {
    removeCaves();
}

cwCavingRegion::RemoveCaveCommand::RemoveCaveCommand(cwCavingRegion* region,
                                                     int beginIndex,
                                                     int endIndex) :
    InsertRemoveCave(region, beginIndex, endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
       Caves.append(region->m_caves[i]);
    }

    QString commandText;
    if(beginIndex != endIndex) {
        commandText = QString("Remove %1 caves").arg(endIndex - beginIndex);
    } else {
        cwCave* cave = region->m_caves[beginIndex];
        commandText = QString("Remove %1").arg(cave->name());
    }
}

void cwCavingRegion::RemoveCaveCommand::redo() {
    removeCaves();
}

void cwCavingRegion::RemoveCaveCommand::undo() {
    insertCaves();
}
