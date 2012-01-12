//Our includes
#include "cwSurveyChunk.h"
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwDebug.h"

//Qt includes
#include <QHash>
#include <QDebug>
#include <QVariant>
#include <QSet>

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

int cwSurveyChunk::stationCount() const {
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

cwStationReference cwSurveyChunk::station(int index) const {
    if(stationIndexCheck(index)) {
        return Stations[index];
    }
    return cwStationReference();
}

int cwSurveyChunk::shotCount() const {
    return Shots.size();
}

cwShot* cwSurveyChunk::shot(int index) const {
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
void cwSurveyChunk::appendNewShot() {
    //Check for special case
    if(!isValid() && Stations.size() <= 2 && Shots.size() <= 1) {
        //Make valid
        for(int i = Stations.size(); i < 2; i++) {

            //Create a new station
            cwStationReference station = createNewStation();

            Stations.append(station);
            emit stationsAdded(i, i);
        }

        if(Shots.size() != 1) {
            Shots.append(new cwShot());
            emit shotsAdded(0, 0);
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

    appendShot(fromStation, toStation, shot);
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
void cwSurveyChunk::appendShot(cwStationReference fromStation, cwStationReference toStation, cwShot* shot) {
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
    emit shotsAdded(index, index);

    index = Stations.size();
    Stations.append(toStation);
    emit stationsAdded(firstIndex, index);
}

/**
  \brief Splits the current survey chunk at stationIndex

  This will split the this chunk into two pieces, if it's valid to split there, ie
  the station Index, is valid.

  This will create a new chunk, that the caller is responsible for deleting
  */
cwSurveyChunk* cwSurveyChunk::splitAtStation(int stationIndex) {
    if(stationIndex < 1 || stationIndex >= Stations.size()) { return NULL; }

    cwSurveyChunk* newChunk = new cwSurveyChunk(this);
    newChunk->Stations.append(createNewStation()); //Add an empty station to the front

    //Copy the points from one chunk to another
    for(int i = stationIndex; i < Stations.size(); i++) {

        //Get the current stations and shots
        cwStationReference station = Stations[i];
        cwShot* currentShot = shot(i - 1);

        newChunk->Stations.append(station);
        if(currentShot != NULL) {
            newChunk->Shots.append(currentShot);
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

    emit stationsRemoved(stationIndex, stationEnd);
    emit shotsRemoved(shotIndex, shotEnd);

    //Append a new last station
    appendNewShot();

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
void cwSurveyChunk::insertStation(int stationIndex, Direction direction) {
    if(Stations.empty()) { appendNewShot(); return; }
    if(stationIndex < 0 || stationIndex >= Stations.size()) { return; }

    int shotIndex = stationIndex;

    if(direction == Below) {
        stationIndex++;
    }

    cwStationReference station = createNewStation();
    cwShot* shot = new cwShot();

    Stations.insert(stationIndex, station);
    Shots.insert(shotIndex, shot);

    emit stationsAdded(stationIndex, stationIndex);
    emit shotsAdded(shotIndex, shotIndex);
}

/**
  Inserts a shot at index stationIndex

  If direction is above, it's inserted at index, if it's below, then this is insert
  at index + 1.  A station will also be added as well
  */
void cwSurveyChunk::insertShot(int shotIndex, Direction direction) {
    if(Stations.empty()) { appendNewShot(); return; }
    if(shotIndex < 0 || shotIndex >= Stations.size()) { return; }

    int stationIndex = shotIndex + 1;

    if(direction == Below) {
        shotIndex++;
    }

    cwStationReference station = createNewStation();
    cwShot* shot = new cwShot();

    Stations.insert(stationIndex, station);
    emit stationsAdded(stationIndex, stationIndex);

    Shots.insert(shotIndex, shot);
    emit shotsAdded(shotIndex, shotIndex);
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
QPair<cwStationReference, cwStationReference> cwSurveyChunk::toFromStations(const cwShot* shot) const {
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
void cwSurveyChunk::removeStation(int stationIndex, Direction shot) {
    if(!canRemoveStation(stationIndex, shot)) { return; }

    //The index to the shot that'll be removed
    int shotIndex = index(stationIndex, shot);

    //Remove them
    remove(stationIndex, shotIndex);
}

/**
  \brief Checks to see if the model can remove the station with the shot direction
  \return true if it can and false if it can't
  */
bool cwSurveyChunk::canRemoveStation(int stationIndex, Direction shot) {
    if(stationCount() <= 2) { return false; }
    if(stationIndex < 0 || stationIndex >= stationCount()) { return false; }
    int shotIndex = index(stationIndex, shot);
    if(shotIndex < 0 || shotIndex >= shotCount()) { return false; }

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
void cwSurveyChunk::removeShot(int shotIndex, Direction station) {
    if(!canRemoveShot(shotIndex, station)) {
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
bool cwSurveyChunk::canRemoveShot(int shotIndex, Direction station) {
    if(shotCount() <= 1) { return false; }
    if(shotIndex < 0 || shotIndex >= shotCount()) { return false; }
    int stationIndex = index(shotIndex, station);
    if(stationIndex < 0 || stationIndex >= stationCount()) { return false; }

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
    if(index < 0 || index >= Shots.size()) { return QVariant(); }

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
  \brief Sets the station's data for role, index, and data
  */
void cwSurveyChunk::setStationData(cwSurveyChunk::DataRole role, int index, const QVariant& data) {
    if(index < 0 || index >= Stations.size()) {
        qDebug() << QString("Can't set station data for role \"%1\" at index: \"%2\" with data: \"%3\"")
                    .arg(role).arg(index).arg(data.toString()) << LOCATION;
        return;
    }

    if(!data.canConvert<QString>()) {
        qDebug() << "Can't convert data to variant" << LOCATION;
        return;
    }

    QString dataString = data.toString();
    cwStationReference& station = Stations[index];

    switch (role) {
    case StationNameRole:
        station.setName(dataString);
        dataChanged(role, index, data);
        break;
    case StationLeftRole:
        station.setLeft(dataString);
        dataChanged(role, index, data);
        break;
    case StationRightRole:
        station.setRight(dataString);
        dataChanged(role, index, data);
        break;
    case StationUpRole:
        station.setUp(dataString);
        dataChanged(role, index, data);
        break;
    case StationDownRole:
        station.setDown(dataString);
        dataChanged(role, index, data);
        break;
    default:
        qDebug() << "Can't find role:" << role << LOCATION;
    }
}

/**
  \brief Sets the shot's data for role, index, and data
  */
void cwSurveyChunk::setShotData(cwSurveyChunk::DataRole role, int index, const QVariant& data) {
    if(index < 0 || index >= Shots.size()) {
        qDebug() << QString("Can't set shot data for role \"%1\" at index: \"%2\" with data: \"%3\"")
                    .arg(role).arg(index).arg(data.toString()) << LOCATION;
        return;
    }

    if(!data.canConvert<QString>()) {
        qDebug() << "Can't convert data to variant" << LOCATION;
        return;
    }

    QString dataString = data.toString();
    cwShot* shot = Shots[index];

    switch(role) {
    case ShotDistanceRole:
        shot->setDistance(dataString);
        dataChanged(role, index, data);
        break;
    case ShotCompassRole:
        shot->setCompass(dataString);
        dataChanged(role, index, data);
        break;
    case ShotBackCompassRole:
        shot->setBackCompass(dataString);
        dataChanged(role, index, data);
        break;
    case ShotClinoRole:
        shot->setClino(dataString);
        dataChanged(role, index, data);
        break;
    case ShotBackClinoRole:
        shot->setBackClino(dataString);
        dataChanged(role, index, data);
        break;
    default:
        qDebug() << "Can't find role:" << role << LOCATION;
    }
}


/**
  \brief Set's the chunk data based on a role
  */
void cwSurveyChunk::setData(DataRole role, int index, QVariant data) {
    switch (role) {
    case StationNameRole:
    case StationLeftRole:
    case StationRightRole:
    case StationUpRole:
    case StationDownRole:
        setStationData(role, index, data);
        break;
    case ShotDistanceRole:
    case ShotCompassRole:
    case ShotBackCompassRole:
    case ShotClinoRole:
    case ShotBackClinoRole:
        setShotData(role, index, data);
        break;
    default:
        qDebug() << "Can't find role:" << role << LOCATION;
    }
}

/**
  \brief Removes a station and a shot from the chunk

  This does no bounds checking!!!
  */
void cwSurveyChunk::remove(int stationIndex, int shotIndex) {
    Stations.removeAt(stationIndex);
    emit stationsRemoved(stationIndex, stationIndex);

    Shots.removeAt(shotIndex);
    emit shotsRemoved(shotIndex, shotIndex);
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

/**
  \brief Returns true if the survey chunk has a station
  */
bool cwSurveyChunk::hasStation(QString stationName) const {

    //Linear search...
    foreach(cwStationReference station, Stations) {
        if(station.name().compare(stationName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;

}

/**
  \brief Returns the neighboring staitions around stationName, that only exist in this
  trip.

  If the survey chunk doesn't have stationName, in this, an empty list
  */
QSet<cwStationReference> cwSurveyChunk::neighboringStations(QString stationName) const {
    if(!hasStation(stationName)) {
        return QSet<cwStationReference>();
    }

    //Get all the indices of the station
    QList<int> stationIndices = indicesOfStation(stationName);

    //Create a unique set of neighboring stations
    QSet<cwStationReference> neighbors;
    foreach(int index, stationIndices) {
        cwStationReference previousShot = station(index - 1);
        cwStationReference nextShot = station(index + 1);

        if(previousShot.isValid()) { neighbors.insert(previousShot); }
        if(nextShot.isValid()) { neighbors.insert(nextShot); }
    }

    return neighbors;
}

/**
  \brief Get's the index of a station based on it's name

  If the station doesn't exist in the survey chunk, this returns an empty list.  Use hasStation()
  to make sure the station exist in the chunk.
  */
QList<int> cwSurveyChunk::indicesOfStation(QString stationName) const {
    QList<int> indices;
    for(int i = 0; i < Stations.size(); i++) {
        if(Stations[i].name().compare(stationName, Qt::CaseInsensitive) == 0) {
            indices.append(i);
        }
    }
    return indices;
}

