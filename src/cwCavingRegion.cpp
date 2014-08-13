/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"

//Qt includes
#include <QDebug>

cwCavingRegion::cwCavingRegion(QObject *parent) :
    QObject(parent)
{
}

/**
  \brief Copy constructor
  */
cwCavingRegion::cwCavingRegion(const cwCavingRegion& object) :
    QObject(nullptr),
    cwUndoer(object.undoStack())
{
    copy(object);
}

/**
  \brief Alignment operator
  */
cwCavingRegion& cwCavingRegion::operator=(const cwCavingRegion& object) {
    return copy(object);
}

/**
  \brief Copy's the object into this object
  */
cwCavingRegion& cwCavingRegion::copy(const cwCavingRegion& object) {
    if(&object == this) {
        return *this;
    }

    //Clear old caves
    int lastIndex = Caves.size() - 1;
    removeCaves(0, lastIndex);

    if(!object.Caves.isEmpty()) {
        emit beginInsertCaves(0, object.Caves.size() - 1);
    }

    //Add new caves
    Caves.reserve(object.Caves.size());
    foreach(cwCave* cave, object.Caves) {

        //Strange copying to make sure the newCaves are
        //On the correct thread
        cwCave* newCave = new cwCave(*cave);

        // FIXME : Test on windows
        //We need to comment this out because it's bad programming,
        //Can't notify other threads with sendEvent, ASSERTs on windows
        newCave->setParent(this);  //Uncomment because this cause problems with QML

        newCave->moveToThread(thread());

        Caves.append(newCave);
    }

    if(Caves.size() - 1 >= 0) {
        emit insertedCaves(0, Caves.size() -1);
        emit caveCountChanged();
    }

    return *this;
}


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
    insertCave(Caves.size(), cave);
}

void cwCavingRegion::addCaves(QList<cwCave*> caves) {
    foreach(cwCave* cave, caves) {
        unparentCave(cave);
        cave->setUndoStack(undoStack());
    }

    //Run the insert cave command
    int firstIndex = Caves.size();
    pushUndo(new InsertCaveCommand(this, caves, firstIndex));
}

/**
  \brief Inserts a cave into the region at inedx
  */
void cwCavingRegion::insertCave(int index, cwCave* cave) {
    if(index < 0 || index > Caves.size()) { return; }

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
    if(beginIndex < 0 || beginIndex >= Caves.size() ||
            endIndex < 0 || endIndex >= Caves.size()) {
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
    if(!Caves.isEmpty()) {
        removeCaves(0, Caves.size() - 1);
    }
}

/**
  \brief Get's the index of the cave
  */
int cwCavingRegion::indexOf(cwCave* cave) {
    return Caves.indexOf(cave);
}

/**
  \brief Sets the undo stack for this region

  This will also set undo stack for the children as well
  */
void cwCavingRegion::setUndoStackForChildren() {
    setUndoStackForChildrenHelper(Caves);
}


/**
  \brief Unparents the cave
  */
void cwCavingRegion::unparentCave(cwCave* cave) {
    //Reparent the trip, if already in another cave
    cwCavingRegion* parentRegion = dynamic_cast<cwCavingRegion*>(((QObject*)cave)->parent());
    if(parentRegion != nullptr) {
        int index = parentRegion->Caves.indexOf(cave);
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
        foreach(cwCave* cave, Caves) {
            // FIXME: double delete for the cave (cause crash)
            cave->deleteLater();
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
    for(int i = 0; i < Caves.size(); i++) {
        int index = BeginIndex + i;
        regionPtr->Caves.insert(index, Caves[i]);
        Caves[i]->setParent(regionPtr);
    }

    OwnsCaves = false;

    emit regionPtr->insertedCaves(BeginIndex, EndIndex);
    emit regionPtr->caveCountChanged();
}

/**
  \brief Removes the caves in this command from the region
  */
void cwCavingRegion::InsertRemoveCave::removeCaves() {
//    if(Region.isNull()) { return; }
    cwCavingRegion* regionPtr = Region; //.data();

    emit regionPtr->beginRemoveCaves(BeginIndex, EndIndex);

    for(int i = Caves.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        regionPtr->Caves.removeAt(index);
        Caves[i]->setParent(nullptr);
    }

    OwnsCaves = true;

    emit regionPtr->removedCaves(BeginIndex, EndIndex);
    emit regionPtr->caveCountChanged();
}


cwCavingRegion::InsertCaveCommand::InsertCaveCommand(cwCavingRegion* parentRegion,
                                                     QList<cwCave*> caves,
                                                     int index) :
    cwCavingRegion::InsertRemoveCave(parentRegion, index, index + caves.size() -1)
{
    Caves = caves;

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
       Caves.append(region->Caves[i]);
    }

    QString commandText;
    if(beginIndex != endIndex) {
        commandText = QString("Remove %1 caves").arg(endIndex - beginIndex);
    } else {
        cwCave* cave = region->Caves[beginIndex];
        commandText = QString("Remove %1").arg(cave->name());
    }
}

void cwCavingRegion::RemoveCaveCommand::redo() {
    removeCaves();
}

void cwCavingRegion::RemoveCaveCommand::undo() {
    insertCaves();
}

