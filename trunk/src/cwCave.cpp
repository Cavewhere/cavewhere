//Our includes
#include "cwCave.h"
#include "cwTrip.h"
#include "cwGlobalUndoStack.h"


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
        cwTrip* newTrip = new cwTrip(*trip); //Deep copy of the trip
        newTrip->setParent(this);
        newTrip->setParentCave(this);
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
        cwGlobalUndoStack::push(new NameCommand(this, name));
    }
}

/**
  Adds a new trip to the cave

  The trip will be added to the end of the all the trip
  */
void cwCave::addTripNullHelper() {
    cwTrip* trip = new cwTrip();

    QString tripName = QString("Trip %1").arg(tripCount() + 1);
    cwGlobalUndoStack::beginMacro(QString("Add %1").arg(tripName));

    trip->setName(tripName);
    addTrip(trip);

    cwGlobalUndoStack::endMacro();
}

/**
  \brief Adds a trip to the cave

  Once the trip is added to the cave, the cave ownes the trip
  */
void cwCave::addTrip(cwTrip* trip) {
    if(trip == NULL) {
        addTripNullHelper();
        return;
    }
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

    cwGlobalUndoStack::push(new InsertTripCommand(this, trip, i));
}

/**
  \brief Removes a trip from the cave

  i - The index where the trip will be remove.  The trip will be the responiblity of
  the caller to delete it
  */
void cwCave::removeTrip(int i) {
    if(i < 0 || i >= Trips.size()) { return; }
    cwGlobalUndoStack::push(new RemoveTripCommand(this, i, i));
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

cwCave::NameCommand::NameCommand(cwCave* cave, QString name) {
    Cave = cave;
    newName = name;
    oldName = Cave->name();
    setText(QString("Change cave's name to %1").arg(name));
}

void cwCave::NameCommand::redo() {
    Cave->Name = newName;
    emit Cave->nameChanged(Cave->Name);
}

void cwCave::NameCommand::undo() {
    Cave->Name = oldName;
    emit Cave->nameChanged(Cave->Name);
}

cwCave::InsertRemoveTrip::InsertRemoveTrip(cwCave* cave,
                                                   int beginIndex, int endIndex) {
    Cave = cave;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
}

void cwCave::InsertRemoveTrip::insertTrips() {
    emit Cave->beginInsertTrips(BeginIndex, EndIndex);
    for(int i = 0; i < Trips.size(); i++) {
        int index = BeginIndex + i;
        Cave->Trips.insert(index, Trips[i]);
        Trips[i]->setParentCave(Cave);
    }
    emit Cave->insertedTrips(BeginIndex, EndIndex);
}

void cwCave::InsertRemoveTrip::removeTrips() {
    emit Cave->beginRemoveTrips(BeginIndex, EndIndex);

    for(int i = Trips.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        Cave->Trips.removeAt(index);
        Trips[i]->setParentCave(NULL);
    }

    emit Cave->removedTrips(BeginIndex, EndIndex);
}


cwCave::InsertTripCommand::InsertTripCommand(cwCave* cave,
                                             QList<cwTrip*> Trips,
                                             int index) :
    cwCave::InsertRemoveTrip(cave, index, index + Trips.size() -1)
{
    Trips = Trips;

    if(Trips.size() == 1) {
        setText(QString("Add %1").arg(Trips.first()->name()));
    } else {
        setText(QString("Add %1 Trips").arg(Trips.size()));
    }
}

cwCave::InsertTripCommand::InsertTripCommand(cwCave* cave,
                                                     cwTrip* Trip,
                                                     int index) :
    cwCave::InsertRemoveTrip(cave, index, index)
{
    Trips.append(Trip);
    setText(QString("Add %1").arg(Trip->name()));
}

cwCave::InsertTripCommand::~InsertTripCommand() {
    foreach(cwTrip* currentTrip, Trips) {
        currentTrip->deleteLater();
    }
}

void cwCave::InsertTripCommand::redo() {
    insertTrips();
}

void cwCave::InsertTripCommand::undo() {
    removeTrips();
}

cwCave::RemoveTripCommand::RemoveTripCommand(cwCave* cave,
                                                     int beginIndex,
                                                     int endIndex) :
    InsertRemoveTrip(cave, beginIndex, endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
       Trips.append(cave->Trips[i]);
    }

    QString commandText;
    if(beginIndex != endIndex) {
        commandText = QString("Remove %1 Trips").arg(endIndex - beginIndex);
    } else {
        cwTrip* Trip = cave->Trips[beginIndex];
        commandText = QString("Remove %1").arg(Trip->name());
    }
}

void cwCave::RemoveTripCommand::redo() {
    removeTrips();
}

void cwCave::RemoveTripCommand::undo() {
    insertTrips();
}

