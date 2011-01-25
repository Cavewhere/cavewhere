//Our includes
#include "cwCave.h"
#include "cwTrip.h"

cwCave::cwCave(QObject* parent) :
    QObject(parent)
{

}

/**
  \brief Copy constructor
  */
cwCave::cwCave(const cwCave& object) :
    QObject(NULL)
{
    Copy(object);
}

/**
  \brief Assignment operator
  */
cwCave& cwCave::operator=(const cwCave& object) {
    return Copy(object);
}

/**
  \brief Copies data from one object to another one
  */
cwCave& cwCave::Copy(const cwCave& object) {
    if(&object == this) {
        return *this;
    }

    //Set the name of the cave
    setName(object.name());

    //Remove all the old trips
    int lastTripIndex = Trips.size() - 1;
    Trips.clear();
    if(lastTripIndex > 0) {
        emit removedTrips(0, lastTripIndex);
    }

    //Copy all the trips from object
    Trips.reserve(object.tripCount());
    for(int i = 0; i < object.tripCount(); i++) {
        cwTrip* trip = object.trip(i);
        cwTrip* newTrip = new cwTrip(*trip); //Deap copy of the trip
        newTrip->setParent(this);
        Trips.append(newTrip);
    }

    if(object.tripCount() - 1 > 0) {
        emit insertedTrips(0, object.tripCount() - 1);
    }

    return *this;


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
void cwCave::addTrip(cwTrip* trip) {
    insertTrip(Trips.size(), trip);
}

/**
  \brief Insert a survey trip to the cave

  i - The index where the trip will be inserted
  */
void cwCave::insertTrip(int i, cwTrip* trip) {
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
    cwTrip* currentTrip = trip(i);
    currentTrip->setParent(NULL);

    Trips.removeAt(i);
    emit removedTrips(i, i);
}

/**
  \brief Removes a station for the lookup

  If the weak pointer is still valid, then this function does nothing
  */
void cwCave::removeStation(QString name) {
    QWeakPointer<cwStation> pointer = StationLookup[name];
    if(pointer.isNull()) {
        StationLookup.remove(name);
    }
}
