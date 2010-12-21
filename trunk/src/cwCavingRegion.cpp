//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"

cwCavingRegion::cwCavingRegion(QObject *parent) :
    QObject(parent)
{
}

/**
  \brief Adds a cave to the region
  */
void cwCavingRegion::addCave(cwCave* cave) {
    insertCave(Caves.size(), cave);
}

void cwCavingRegion::addCaves(QList<cwCave*> caves) {
    int firstIndex = Caves.size();

    foreach(cwCave* cave, caves) {
        insertHelper(Caves.size(), cave);
    }

    emit insertedCaves(firstIndex, Caves.size() - 1);
}

/**
  \brief Inserts a cave into the region at inedx
  */
void cwCavingRegion::insertCave(int index, cwCave* cave) {
    if(index < 0 || index > Caves.size()) { return; }

    insertHelper(index, cave);

    emit insertedCaves(index, index);
}

/**
  \brief Removes the cave at index
  */
void cwCavingRegion::removeCave(int index) {
    if(index > 0 || index >= Caves.size()) { return; }

    //Unparent the trip
    cwCave* currentCave = cave(index);
    currentCave->setParent(NULL);

    Caves.removeAt(index);
    emit removedCaves(index, index);
}

/**
  \brief Get's the index of the cave
  */
int cwCavingRegion::indexOf(cwCave* cave) {
    return Caves.indexOf(cave);
}

/**
  \brief Helps the inserter
  */
void cwCavingRegion::insertHelper(int index, cwCave* cave) {
    //Reparent the trip, if already in another cave
    cwCavingRegion* parentRegion = dynamic_cast<cwCavingRegion*>(((QObject*)cave)->parent());
    if(parentRegion != NULL) {
        int index = parentRegion->Caves.indexOf(cave);
        parentRegion->removeCave(index);
    }

    Caves.insert(index, cave);
    cave->setParent(this);
}
