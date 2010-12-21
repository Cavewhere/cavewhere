//Our includes
#include "cwCave.h"
#include "cwSurveyTrip.h"

cwCave::cwCave(QObject* parent) :
    QObject(parent)
{

}

/**
  \brief Sets the name of the cwCave
  */
void cwCave::setName(QString name) {
    if(Name != name) {
        Name = name;
        emit nameChanged(Name);
    }
}

/**
  \brief Adds a trip to the cave

  Once the trip is added to the cave, the cave ownes the trip
  */
void cwCave::addTrip(cwSurveyTrip* trip) {
    insertTrip(Trips.size(), trip);
}

/**
  \brief Insert a survey trip to the cave

  i - The index where the trip will be inserted
  */
void cwCave::insertTrip(int i, cwSurveyTrip* trip) {
    if(i < 0 || i > Trips.size()) { return; }

    //Reparent the trip, if already in another cave
    cwCave* parentCave = dynamic_cast<cwCave*>(((QObject*)trip)->parent());
    if(parentCave != NULL) {
        int index = parentCave->Trips.indexOf(trip);
        parentCave->removeTrip(index);
    }

    Trips.insert(i, trip);
    trip->setParent(this);

    emit insertedTrips(i, i);
}

/**
  \brief Removes a trip from the cave

  i - The index where the trip will be remove.  The trip will be the responiblity of
  the caller to delete it
  */
void cwCave::removeTrip(int i) {
    if(i > 0 || i >= Trips.size()) { return; }

    //Unparent the trip
    cwSurveyTrip* currentTrip = trip(i);
    currentTrip->setParent(NULL);

    Trips.removeAt(i);
    emit removedTrips(i, i);
}
