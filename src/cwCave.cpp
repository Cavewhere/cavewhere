//Our includes
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwLength.h"

cwCave::cwCave(QObject* parent) :
    QObject(parent),
    Length(new cwLength(this)),
    Depth(new cwLength(this))
{
    Length->setUnit(cwUnits::Meters);
    Depth->setUnit(cwUnits::Meters);

    Length->setUpdateValue(true);
    Depth->setUpdateValue(true);
}

/**
  \brief Copy constructor
  */
cwCave::cwCave(const cwCave& object) :
    QObject(NULL),
    cwUndoer(),
    Length(new cwLength(this)),
    Depth(new cwLength(this))
{
    Copy(object);
}

/**
  \brief Assignment operator
  */
cwCave& cwCave::operator=(const cwCave& object) {
    return Copy(object);
}

cwCave::~cwCave() {
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

    //Set the station positions
    setStationPositionLookup(object.stationPositionLookup());

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

    *Length = *(object.Length);
    *Depth = *(object.Depth);

    return *this;
}

/**
  \brief Sets the name of the cwCave
  */
void cwCave::setName(QString name) {
    if(Name != name) {
        pushUndo(new NameCommand(this, name));
    }
}



/**
  Adds a new trip to the cave

  The trip will be added to the end of the all the trip
  */
void cwCave::addTripNullHelper() {
    cwTrip* trip = new cwTrip(undoStack());

    QString tripName = QString("Trip %1").arg(tripCount() + 1);
    beginUndoMacro(QString("Add %1").arg(tripName));

    trip->setName(tripName);
    addTrip(trip);

    endUndoMacro();
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

    pushUndo(new InsertTripCommand(this, trip, i));
}

/**
  \brief Removes a trip from the cave

  i - The index where the trip will be remove.  The trip will be the responiblity of
  the caller to delete it
  */
void cwCave::removeTrip(int i) {
    if(i < 0 || i >= Trips.size()) { return; }
    pushUndo(new RemoveTripCommand(this, i, i));
}

///**
//  \brief Removes a station for the lookup

//  If the weak pointer is still valid, then this function does nothing
//  */
//void cwCave::removeStation(QString name) {
//    QWeakPointer<cwStation> pointer = StationLookup[name];
//    if(pointer.isNull()) {
//        StationLookup.remove(name);
//    }
//}

///**
//  \brief This sets the station's data based on it's role

//  This creates a undo / redo command and pushes it onto the undo stack
//  */
//void cwCave::setStationData(QSharedPointer<cwStation> station,
//                            QVariant data,
//                            cwStation::DataRoles role) {

//    station->setData(data, role);
//}

///**
//  \brief This gets the stations data base on the role
//  */
//QVariant cwCave::stationData(QSharedPointer<cwStation> station, cwStation::DataRoles role) const {
//    return station->data(role);
//}

/**
  \brief Sets the undo stack for the cave and all of it's children
  */
void cwCave::setUndoStackForChildren() {
    setUndoStackForChildrenHelper(Trips);
}


cwCave::NameCommand::NameCommand(cwCave* cave, QString name) {
    CavePtr = QWeakPointer<cwCave>(cave);
    newName = name;
    oldName = cave->name();
    setText(QString("Change cave's name to %1").arg(name));
}

void cwCave::NameCommand::redo() {
    if(CavePtr.isNull()) { return; }
    cwCave* cave = CavePtr.data();
    cave->Name = newName;
    emit cave->nameChanged(cave->Name);
}


void cwCave::NameCommand::undo() {
    if(CavePtr.isNull()) { return; }
    cwCave* cave = CavePtr.data();
    cave->Name = oldName;
    emit cave->nameChanged(cave->Name);
}

cwCave::InsertRemoveTrip::InsertRemoveTrip(cwCave* cave,
                                                   int beginIndex, int endIndex) {
    CavePtr = cave;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
    OwnsTrips = false;
}

cwCave::InsertRemoveTrip::~InsertRemoveTrip() {
    if(OwnsTrips) {
        foreach(cwTrip* trip, Trips) {
            trip->deleteLater();
        }
    }
}

void cwCave::InsertRemoveTrip::insertTrips() {
    if(CavePtr.isNull()) { return; }
    cwCave* cave = CavePtr.data();
    emit cave->beginInsertTrips(BeginIndex, EndIndex);
    for(int i = 0; i < Trips.size(); i++) {
        int index = BeginIndex + i;
        cave->Trips.insert(index, Trips[i]);
        Trips[i]->setParentCave(cave);
    }

    OwnsTrips = false;

    emit cave->insertedTrips(BeginIndex, EndIndex);
}

void cwCave::InsertRemoveTrip::removeTrips() {
    if(CavePtr.isNull()) { return; }
    cwCave* cave = CavePtr.data();
    emit cave->beginRemoveTrips(BeginIndex, EndIndex);

    //Remove all the trips from the back to the front
    for(int i = Trips.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        cave->Trips.removeAt(index);
        Trips[i]->setParentCave(NULL);
    }

    OwnsTrips = true;

    emit cave->removedTrips(BeginIndex, EndIndex);
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

//cwCave::StationDataCommand::StationDataCommand(cwCave* cave,
//        QSharedPointer<cwStation> station,
//        QVariant data,
//        cwStation::DataRoles role) {
//    Cave = cave;
//    Station = station;
//    OldData = station->data(role);
//    NewData = data;
//    Role = role;

//    QString commandText;
//    switch(role) {
//    case cwStation::NameRole:
//        commandText = QString("Stations Name - %1").arg(data.toString());
//        break;
//    case cwStation::LeftRole:
//        commandText = QString("Station %1's left to %1").arg(station->name()).arg(data.toString());
//        break;
//    case cwStation::RightRole:
//        commandText = QString("Station %1's right to %1").arg(station->name()).arg(data.toString());
//        break;
//    case cwStation::UpRole:
//        commandText = QString("Station %1's up to %1").arg(station->name()).arg(data.toString());
//        break;
//    case cwStation::DownRole:
//        commandText = QString("Station %1's down to %1").arg(station->name()).arg(data.toString());
//        break;
////    case cwStation::PositionRole: {
////        QVector3D position = data.value<QVector3D>();
////        commandText = QString("Station %1's position to %1, %2, %3")
////                .arg(station->name())
////                .arg(position.x())
////                .arg(position.y())
////                .arg(position.z());
////        break;
////    }
//    }
//    setText(commandText);
//}

//void cwCave::StationDataCommand::redo() {
//    if(Cave.isNull()) { return; }
//    Station->setData(NewData, Role);
//    emit Cave.data()->stationDataChanged(Station, Role);
//}

//void cwCave::StationDataCommand::undo() {
//    if(Cave.isNull()) { return; }
//    Station->setData(OldData, Role);
//    emit Cave.data()->stationDataChanged(Station, Role);
//}

/**
  \brief Gets all the stations in the cave

  To figure out how stations are structured see cwTrip and cwSurveyChunk

  Shots and stations are stored in the cwSurveyChunk
  */
QList<cwStation> cwCave::stations() const {
    QList<cwStation> allStations;
    foreach(cwTrip* trip, trips()) {
        allStations.append(trip->stations());
    }
    return allStations;
}

/**
  \brief Sets the station position model for the cave

  This holds all the position for al the stations in the cave (this is populated
  after the loop closure has completed)
  */
void cwCave::setStationPositionLookup(const cwStationPositionLookup &model) {
    StationPositionModel = model;

    //FIXME: This call is really expensive because every time, the user change the
    // line plot, all the scrap transformations have to be updated.
    //Go through all the notes and update the automatic scrap transfrom for all scraps
    //This is probably the more stable way to do this, but it's slow
    //    foreach(cwTrip* trip, trips()) {
    //        trip->stationPositionModelUpdated();
    //    }

    emit stationPositionPositionChanged();
}
