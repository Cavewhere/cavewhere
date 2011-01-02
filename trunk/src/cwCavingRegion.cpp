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

    for(int i = endIndex; i >= beginIndex; i--) {
        //Unparent the trip
        cwCave* currentCave = cave(i);
        currentCave->deleteLater();

        Caves.removeAt(i);
    }

    emit removedCaves(beginIndex, endIndex);
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
