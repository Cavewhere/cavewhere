//Our includes
#include "cwSurveyChunk.h"
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwTrip.h"
#include "cwCave.h"

//Qt includes
#include <QHash>
#include <QDebug>
#include <QVariant>

cwSurveyChunk::cwSurveyChunk(QObject * parent) :
    QObject(parent),
    ParentTrip(NULL)
{

}

/**
  \brief Makes a deap copy of the chunk

  All station will be copied into new stations, with the same data.
  */
cwSurveyChunk::cwSurveyChunk(const cwSurveyChunk& chunk) :
    QObject(),
    ParentTrip(NULL)
{

    //Copy all the stations
    Stations = chunk.Stations;
//    Stations.reserve(chunk.Stations.size());
//    foreach(cwStationReference station, chunk.Stations) {
//        cwStationReference* newStation = new cwStationReference(*station);
//        Stations.append(newStation);
//    }

    //Copy all the shots
    Shots.reserve(chunk.Shots.size());
    foreach(cwShot* shot, chunk.Shots) {
        cwShot* newShot = new cwShot(*shot);
        newShot->setParentChunk(this);
        Shots.append(newShot);
    }
}

cwSurveyChunk::~cwSurveyChunk() {
    foreach(cwShot* shot, Shots) {
        delete shot;
    }
}

/**
  \brief Checks if the survey Chunk is valid
  */
bool cwSurveyChunk::isValid() const {
    return (Stations.size() - 1) == Shots.size() && !Stations.empty() && !Shots.empty();
}

/**
  \brief Gets the parent cave for this chunk
  */
cwCave* cwSurveyChunk::parentCave() const {
    return parentTrip() != NULL ? parentTrip()->parentCave() : NULL;
}

int cwSurveyChunk::StationCount() const {
    return Stations.count();
}

/**
  \brief Sets the parent trip for this chunk
  */
void cwSurveyChunk::setParentTrip(cwTrip* trip) {
    if(ParentTrip != trip) {
        ParentTrip = trip;
        setParent(trip);
        updateStationsWithNewCave();
    }
}

cwStationReference cwSurveyChunk::Station(int index) const {
    if(stationIndexCheck(index)) {
        return Stations[index];
    }
    return cwStationReference();
}

int cwSurveyChunk::ShotCount() const {
    return Shots.size();
}

cwShot* cwSurveyChunk::Shot(int index) const {
    if(shotIndexCheck(index)) {
        return Shots[index];
    }
    return NULL;
}

/**
  \brief Adds a station to the survey chunk.

  This will add the station to the end of the list.
  This will also add a shot to the suvey chunk.
  */
void cwSurveyChunk::AppendNewShot() {
    //Check for special case
    if(!isValid() && Stations.size() <= 2 && Shots.size() <= 1) {
        //Make valid
        for(int i = Stations.size(); i < 2; i++) {

            //Create a new station
            cwStationReference station = createNewStation();

            Stations.append(station);
            emit StationsAdded(i, i);
        }

        if(Shots.size() != 1) {
            Shots.append(new cwShot());
            emit ShotsAdded(0, 0);
        }
        return;
    }


    cwStationReference fromStation;
    if(Stations.isEmpty()) {
        fromStation = createNewStation();
    } else {
        fromStation = Stations.last();
        if(!fromStation.isValid()) {
            return;
        }
    }

    cwStationReference toStation = createNewStation();
    cwShot* shot = new cwShot();

    AppendShot(fromStation, toStation, shot);
}

/**
  \brief Adds a shot to the chunk

  \param fromStation - The shot's fromStation
  \param toStation - The shot's toStation
  \param shot - The shot

  Use CanAddShot to make sure you can add the shot.
  This function does nothing if the from, to station, or shot are null or the from station
  isn't equal to the last station in the chunk.  If toStation isn't the last station,
  you need to create a new cwSurveyChunk and call this function again
  */
void cwSurveyChunk::AppendShot(cwStationReference fromStation, cwStationReference toStation, cwShot* shot) {
    //qDebug() << "Trying to add shot";
    if(!canAddShot(fromStation, toStation, shot)) { return; }

    int index;
    int firstIndex = Stations.size();
    if(Stations.empty()) {
        Stations.append(fromStation);
    }

    index = Shots.size();
    Shots.append(shot);
    shot->setParentChunk(this);
    emit ShotsAdded(index, index);

    index = Stations.size();
    Stations.append(toStation);
    emit StationsAdded(firstIndex, index);
}

/**
  \brief Splits the current survey chunk at stationIndex

  This will split the this chunk into two pieces, if it's valid to split there, ie
  the station Index, is valid.

  This will create a new chunk, that the caller is responsible for deleting
  */
cwSurveyChunk* cwSurveyChunk::SplitAtStation(int stationIndex) {
    if(stationIndex < 1 || stationIndex >= Stations.size()) { return NULL; }

    cwSurveyChunk* newChunk = new cwSurveyChunk(this);
    newChunk->Stations.append(createNewStation()); //Add an empty station to the front

    //Copy the points from one chunk to another
    for(int i = stationIndex; i < Stations.size(); i++) {

        //Get the current stations and shots
        cwStationReference station = Stations[i];
        cwShot* shot = Shot(i - 1);

        newChunk->Stations.append(station);
        if(shot != NULL) {
            newChunk->Shots.append(shot);
        }
    }

    int stationEnd = Stations.size() - 1;
    int shotEnd = Shots.size() - 1;


    //Remove the stations and shots from the list
    int shotIndex = stationIndex - 1;
    QList<cwStationReference>::iterator stationIter = Stations.begin() + stationIndex;
    QList<cwShot*>::iterator shotIter = Shots.begin() + shotIndex;
    Stations.erase(stationIter, Stations.end());
    Shots.erase(shotIter, Shots.end());

    emit StationsRemoved(stationIndex, stationEnd);
    emit ShotsRemoved(shotIndex, shotEnd);

    //Append a new last station
    AppendNewShot();

    return newChunk;
}


/**
  Inserts a station a index stationIndex.

  If direction is above, it's inserted at index, if it's below, then this is insert
  at index + 1.  A shot will also be added as well

  \brief stationIndex - The index where station and shots will be inserted
  \brief direction - The directon that the stations will be inserted in
  \param count - The number of station and shots that'll be inserted

  */
void cwSurveyChunk::InsertStation(int stationIndex, Direction direction) {
    if(Stations.empty()) { AppendNewShot(); return; }
    if(stationIndex < 0 || stationIndex >= Stations.size()) { return; }

    int shotIndex = stationIndex;

    if(direction == Below) {
        stationIndex++;
    }

    cwStationReference station = createNewStation();
    cwShot* shot = new cwShot();

    Stations.insert(stationIndex, station);
    Shots.insert(shotIndex, shot);

    emit StationsAdded(stationIndex, stationIndex);
    emit ShotsAdded(shotIndex, shotIndex);
}

/**
  Inserts a shot at index stationIndex

  If direction is above, it's inserted at index, if it's below, then this is insert
  at index + 1.  A station will also be added as well
  */
void cwSurveyChunk::InsertShot(int shotIndex, Direction direction) {
    if(Stations.empty()) { AppendNewShot(); return; }
    if(shotIndex < 0 || shotIndex >= Stations.size()) { return; }

    int stationIndex = shotIndex + 1;

    if(direction == Below) {
        shotIndex++;
    }

    cwStationReference station = createNewStation();
    cwShot* shot = new cwShot();

    Stations.insert(stationIndex, station);
    emit StationsAdded(stationIndex, stationIndex);

    Shots.insert(shotIndex, shot);
    emit ShotsAdded(shotIndex, shotIndex);
}


/**
  \brief Checks if a shot can be added to the chunk

  \returns true if AddShot() will be successfull or will do nothing
  */
bool cwSurveyChunk::canAddShot(const cwStationReference& fromStation, const cwStationReference& toStation, cwShot* shot) {
    return !fromStation.station().isNull() &&
            !toStation.station().isNull() &&
            shot != NULL &&
            (Stations.empty() || Stations.last() == fromStation);
}

/**
  \brief Returns the two and from stations at shot

  This first find the shot and the get's the to and from station.
  */
QPair<cwStationReference, cwStationReference> cwSurveyChunk::ToFromStations(const cwShot* shot) const {
    if(!isValid()) { return QPair<cwStationReference, cwStationReference>(); }
    //if(shot->parent() != this) { return QPair<cwStationReference*, cwStationReference*>(NULL, NULL); }

    for(int i = 0; i < Shots.size(); i++) {
        if(Shots[i] == shot) {
            return QPair<cwStationReference, cwStationReference>(Stations[i], Stations[i + 1]);
        }
    }

    return QPair<cwStationReference, cwStationReference>();
}


/**
  \brief Removes a shot and a station from the chunk

  This will do nothing if the stationIndex is out of bounds.
  \param stationIndex - The station that'll be removed from the model
  \param shot - The shot that'll be removed.  The shot above the station or
  below the station will be remove.  If the shot direction is invalid, ie. can't
  remove the shot, this function will do nothing.
  */
void cwSurveyChunk::RemoveStation(int stationIndex, Direction shot) {
    if(!CanRemoveStation(stationIndex, shot)) { return; }

    //The index to the shot that'll be removed
    int shotIndex = index(stationIndex, shot);

    //Remove them
    remove(stationIndex, shotIndex);
}

/**
  \brief Checks to see if the model can remove the station with the shot direction
  \return true if it can and false if it can't
  */
bool cwSurveyChunk::CanRemoveStation(int stationIndex, Direction shot) {
    if(StationCount() <= 2) { return false; }
    if(stationIndex < 0 || stationIndex >= StationCount()) { return false; }
    int shotIndex = index(stationIndex, shot);
    if(shotIndex < 0 || shotIndex >= ShotCount()) { return false; }

    return true;
}

/**
  \brief Removes a shot and a station from the chunk

  This will do nothing if the shotIndex is out of bounds.
  \param shotIndex - The station that'll be removed from the model
  \param station - The station that'll be removed.  The station above the shot or
  below the shot will be remove.  If the shot direction is invalid, ie. can't
  remove the station, this function will do nothing.
  */
void cwSurveyChunk::RemoveShot(int shotIndex, Direction station) {
    if(!CanRemoveShot(shotIndex, station)) {
        return;
    }

    //The index of the station that'll be removed
    int stationIndex = index(shotIndex, station);

    //Remove them
    remove(stationIndex, shotIndex);
}

/**
  \brief Checks to see if the model can remove the shot with the station direction
  \return true if it can and false if it can't
  */
bool cwSurveyChunk::CanRemoveShot(int shotIndex, Direction station) {
    if(ShotCount() <= 1) { return false; }
    if(shotIndex < 0 || shotIndex >= ShotCount()) { return false; }
    int stationIndex = index(shotIndex, station);
    if(stationIndex < 0 || stationIndex >= StationCount()) { return false; }

    return true;
}


/**
  \brief Get's the chunk data based on a role
  */
QVariant cwSurveyChunk::data(DataRole role, int index) const {

    switch (role) {
    case StationNameRole:
    case StationLeftRole:
    case StationRightRole:
    case StationUpRole:
    case StationDownRole:
        return stationData(role, index);
    case ShotDistanceRole:
    case ShotCompassRole:
    case ShotBackCompassRole:
    case ShotClinoRole:
    case ShotBackClinoRole:
        return shotData(role, index);

    default:
        return QVariant();
    }
}

/**
  \brief Helper function to data
  */
QVariant cwSurveyChunk::stationData(DataRole role, int index) const {
    if(index < 0 || index >= Stations.size()) { return QVariant(); }

    const cwStationReference& station = Stations[index];

    switch (role) {
    case StationNameRole:
        return station.name();
    case StationLeftRole:
        return station.left();
    case StationRightRole:
        return station.right();
    case StationUpRole:
        return station.up();
    case StationDownRole:
        return station.down();
    default:
        return QVariant();
    }
}

/**
  \brief Helper function to data
  */
QVariant cwSurveyChunk::shotData(DataRole role, int index) const {
    if(index < 0 || index >= Stations.size()) { return QVariant(); }

    cwShot* shot = Shots[index];

    switch(role) {
    case ShotDistanceRole:
        return shot->distance();
    case ShotCompassRole:
        return shot->compass();
    case ShotBackCompassRole:
        return shot->backCompass();
    case ShotClinoRole:
        return shot->clino();
    case ShotBackClinoRole:
        return shot->backClino();
    default:
        return QVariant();
    }
}


/**
  \brief Set's the chunk data based on a role
  */
void cwSurveyChunk::setData(DataRole /*role*/, int /*index*/, QVariant /*data*/) {
   // qDebug() << "Role:" << role << "Index:" << index << "Data:" << data;
}

/**
  \brief Removes a station and a shot from the chunk

  This does no bounds checking!!!
  */
void cwSurveyChunk::remove(int stationIndex, int shotIndex) {
    Stations.removeAt(stationIndex);
    emit StationsRemoved(stationIndex, stationIndex);

    Shots.removeAt(shotIndex);
    emit ShotsRemoved(shotIndex, shotIndex);
}

/**
  \brief Helper to the remove functions.
  */
int cwSurveyChunk::index(int index, Direction direction) {
    switch(direction) {
    case Above:
        return index - 1;
    case Below:
        return index;
    }
    return -1;
}

/**
  Uses the parent trip and updates all the stations in this chunk
  with the new cave.

  If the parent trip is null or parent trip's cave is null, this
  does nothing
  */
void cwSurveyChunk::updateStationsWithNewCave() {
    if(ParentTrip == NULL || ParentTrip->parentCave() == NULL) { return; }

    for(int i = 0; i < Stations.size(); i++) {
        cwCave* cave = ParentTrip->parentCave();
        Stations[i].setCave(cave);
    }
}

/**
  \brief This creates a new station in the chunk

  The station will be owned by this chunk, and the parent cave will be set
  for the station
  */
cwStationReference cwSurveyChunk::createNewStation() {
    //Create a new station
    cwStationReference station; // = new cwStationReference();
    station.setCave(parentCave());
    return station;
}



