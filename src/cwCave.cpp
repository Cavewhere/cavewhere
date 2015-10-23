/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwLength.h"
#include "cwErrorModel.h"

cwCave::cwCave(QObject* parent) :
    QAbstractListModel(parent),
    Length(new cwLength(this)),
    Depth(new cwLength(this)),
    ErrorModel(new cwErrorModel(this)),
    StationPositionModelStale(false)
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
    QAbstractListModel(nullptr),
    cwUndoer(),
    Length(new cwLength(this)),
    Depth(new cwLength(this)),
    ErrorModel(new cwErrorModel(this)),
    StationPositionModelStale(false)
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

    StationPositionModelStale = object.StationPositionModelStale;

    return *this;
}

/**
  \brief Sets the name of the cwCave
  */
void cwCave::setName(QString name) {
    if(Name != name && !name.isEmpty()) {
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
    if(trip == nullptr) {
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
    if(parentCave != nullptr) {
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

/**
 * @brief cwCave::rowCount
 * @param parent
 * @return Returns the number of trips in the cwCave
 */
int cwCave::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return tripCount();
}

/**
 * @brief cwCave::data
 * @param index
 * @param role
 * @return
 */
QVariant cwCave::data(const QModelIndex &index, int role) const
{
   if(!index.isValid()) {
       return QVariant();
   }

   switch(role) {
   case TripObjectRole:
       return QVariant::fromValue(Trips.at(index.row()));
   default:
       return QVariant();
   }

   return QVariant();
}

/**
 * @brief cwCave::roleNames
 * @return Returns the roleNames of the model. See the Qt doc for details
 */
QHash<int, QByteArray> cwCave::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TripObjectRole, "tripObjectRole");
    return roles;
}

/**
 * @brief cwCave::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwCave::index(int row, int column, const QModelIndex &parent) const
{
   return QAbstractListModel::index(row, column, parent);
}

/**
  \brief Sets the undo stack for the cave and all of it's children
  */
void cwCave::setUndoStackForChildren() {
    setUndoStackForChildrenHelper(Trips);
}


cwCave::NameCommand::NameCommand(cwCave* cave, QString name) {
    CavePtr = cave;
    newName = name;
    oldName = cave->name();
    setText(QString("Change cave's name to %1").arg(name));
}

void cwCave::NameCommand::redo() {
    cwCave* cave = CavePtr; //.data();
    cave->Name = newName;
    emit cave->nameChanged();
}


void cwCave::NameCommand::undo() {
    cwCave* cave = CavePtr; //.data();
    cave->Name = oldName;
    emit cave->nameChanged();
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
    cwCave* cave = CavePtr; //.data();
    emit cave->beginInsertTrips(BeginIndex, EndIndex);
    emit cave->beginInsertRows(QModelIndex(), BeginIndex, EndIndex);
    for(int i = 0; i < Trips.size(); i++) {
        int index = BeginIndex + i;
        cave->Trips.insert(index, Trips[i]);
        Trips[i]->setParentCave(cave);
    }

    OwnsTrips = false;

    emit cave->endInsertRows();
    emit cave->insertedTrips(BeginIndex, EndIndex);
}

void cwCave::InsertRemoveTrip::removeTrips() {
    cwCave* cave = CavePtr; //.data();
    emit cave->beginRemoveTrips(BeginIndex, EndIndex);
    emit cave->beginRemoveRows(QModelIndex(), BeginIndex, EndIndex);

    //Remove all the trips from the back to the front
    for(int i = Trips.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        cave->Trips.removeAt(index);
        Trips[i]->setParentCave(nullptr);
    }

    OwnsTrips = true;

    emit cave->endRemoveRows();
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

    emit stationPositionPositionChanged();
}

/**
 * @brief cwCave::setStationPositionLookupStale
 * @param isStale
 *
 * Sets the station position lookup as stale. If true, the station position lookup, should
 * be recalculated, else, it shouldn't be recalculated.
 */
void cwCave::setStationPositionLookupStale(bool isStale)
{
    StationPositionModelStale = isStale;
}

/**
 * @brief cwCave::isStationPositionLookupStale
 * @return True if the station position lookup is old and stale, false it is up to date
 */
bool cwCave::isStationPositionLookupStale() const
{
    return StationPositionModelStale;
}
