//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwGlobalUndoStack.h"

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
    QObject(NULL)
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

    //Add new caves
    Caves.reserve(object.Caves.size());
    foreach(cwCave* cave, object.Caves) {
        cwCave* newCave = new cwCave(*cave);
        newCave->setParent(this);
        Caves.append(newCave);
    }

    if(Caves.size() - 1 > 0) {
        emit insertedCaves(0, Caves.size() -1);
    }

    return *this;
}

/**
  \brief Creates a new cave and adds it to the caving region
  */
void cwCavingRegion::addCave() {
    QString newCaveName = QString("Cave %1").arg(caveCount() + 1);
    cwGlobalUndoStack::beginMacro(QString("Add %1").arg(newCaveName));

    cwCave* cave = new cwCave();
    cave->setName(newCaveName);
    addCave(cave);

    cwGlobalUndoStack::endMacro();
}

/**
  \brief Adds a cave to the region
  */
void cwCavingRegion::addCave(cwCave* cave) {
    insertCave(Caves.size(), cave);
}

void cwCavingRegion::addCaves(QList<cwCave*> caves) {


    foreach(cwCave* cave, caves) {
        unparentCave(cave);
    }

    //Run the insert cave command
    int firstIndex = Caves.size();
    cwGlobalUndoStack::push(new InsertCaveCommand(this, caves, firstIndex));
}

/**
  \brief Inserts a cave into the region at inedx
  */
void cwCavingRegion::insertCave(int index, cwCave* cave) {
    if(index < 0 || index > Caves.size()) { return; }

    unparentCave(cave);

    //Run the insert cave command
    cwGlobalUndoStack::push(new InsertCaveCommand(this, cave, index));
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

    cwGlobalUndoStack::push(new RemoveCaveCommand(this, beginIndex, endIndex));
}

/**
  \brief Get's the index of the cave
  */
int cwCavingRegion::indexOf(cwCave* cave) {
    return Caves.indexOf(cave);
}

/**
  \brief Unparents the cave
  */
void cwCavingRegion::unparentCave(cwCave* cave) {
    //Reparent the trip, if already in another cave
    cwCavingRegion* parentRegion = dynamic_cast<cwCavingRegion*>(((QObject*)cave)->parent());
    if(parentRegion != NULL) {
        int index = parentRegion->Caves.indexOf(cave);
        parentRegion->removeCave(index);
    }
}

cwCavingRegion::InsertRemoveCave::InsertRemoveCave(cwCavingRegion* region,
                                                   int beginIndex, int endIndex) {
    Region = region;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
}

void cwCavingRegion::InsertRemoveCave::insertCaves() {
    emit Region->beginInsertCaves(BeginIndex, EndIndex);
    for(int i = 0; i < Caves.size(); i++) {
        int index = BeginIndex + i;
        Region->Caves.insert(index, Caves[i]);
        Caves[i]->setParent(Region);
    }
    emit Region->insertedCaves(BeginIndex, EndIndex);
}

void cwCavingRegion::InsertRemoveCave::removeCaves() {
    emit Region->beginRemoveCaves(BeginIndex, EndIndex);

    for(int i = Caves.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        Region->Caves.removeAt(index);
        Caves[i]->setParent(NULL);
    }

    emit Region->removedCaves(BeginIndex, EndIndex);
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

cwCavingRegion::InsertCaveCommand::~InsertCaveCommand() {
    foreach(cwCave* currentCave, Caves) {
        currentCave->deleteLater();
    }
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

