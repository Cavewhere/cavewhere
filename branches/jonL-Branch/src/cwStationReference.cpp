//Our includes
#include "cwStationReference.h"
#include "cwCave.h"

cwStationReference::cwStationReference() :
    Cave(NULL),
    SharedStation(new cwStation())
{

}

cwStationReference::cwStationReference(QString name) :
    Cave(NULL),
    SharedStation(new cwStation(name))
{

}

/**
  \brief Sets the cave for this referance

  If the station already exist in the cave, then the data will
  be over written with new data
  */
void cwStationReference::setCave(cwCave* cave) {
    if(Cave == cave) { return; }

    Cave = cave;

    //The cave already has a station named this
    if(Cave->hasStation(name())) {
        //Overwrite that station
        QSharedPointer<cwStation> caveStation = cave->station(name()).toStrongRef();

        //Copy the data from this object
        caveStation->setDown(SharedStation->down());
        caveStation->setLeft(SharedStation->left());
        caveStation->setRight(SharedStation->right());
        caveStation->setUp(SharedStation->up());
        caveStation->setPosition(SharedStation->position());

        //Set that station to this object
        SharedStation = caveStation;
    } else {
        //Add this station to the cave
        Cave->addStation(SharedStation);
    }
}

/**
  \brief Updates the name

  If the name has changed, this will lookup the station.  If the station exist, then
  this station will be update with the existing station's data.  If the station doesn't exist
  then this station will be renamed.
  */
void cwStationReference::setName(QString newName) {
    if(newName == name()) { return; }

    if(Cave != NULL) {
        //We have cave
        if(Cave->hasStation(newName)) {
            SharedStation = Cave->station(newName);
        } else {
            //Remove it from the cave

            //Make the temperary point to the data
            QString oldName = SharedStation->name();

            //Create a new station
            cwStation* newStation = new cwStation(newName);

            //Reassign the station
            SharedStation = QSharedPointer<cwStation>(newStation);

            //Try to remove the old station
            Cave->removeStation(oldName);

            //Try to add the new station
            Cave->addStation(SharedStation);
        }
    } else {
        //No cave attached, so just set the sharedStation
        SharedStation->setName(newName);
    }
}

void cwStationReference::setLeft(QString left) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setLeft(left);
}

void cwStationReference::setRight(QString right) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setRight(right);
}

void cwStationReference::setUp(QString up) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setUp(up);
}

void cwStationReference::setDown(QString down) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setDown(down);
}

void cwStationReference::setPosition(QVector3D position) {
    Q_ASSERT(!SharedStation.isNull());
    SharedStation->setPosition(position);
}


