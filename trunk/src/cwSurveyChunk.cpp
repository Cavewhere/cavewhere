//Our includes
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"


//Qt includes
#include <QHash>
#include <QDebug>
#include <QVariant>

cwSurveyChunk::cwSurveyChunk(QObject * parent) :
        QObject(parent)
{

}

/**
  \brief Makes a deap copy of the chunk

  All station will be copied into new stations, with the same data.
  */
cwSurveyChunk::cwSurveyChunk(const cwSurveyChunk& chunk) : QObject() {

    //Copy all the stations
    Stations.reserve(chunk.Stations.size());
    foreach(cwStation* station, chunk.Stations) {
        cwStation* newStation = new cwStation(*station);
        Stations.append(newStation);
    }

    //Copy all the shots
    Shots.reserve(chunk.Shots.size());
    foreach(cwShot* shot, chunk.Shots) {
        cwShot* newShot = new cwShot(*shot);
        newShot->setParent(this);
        Shots.append(newShot);
    }
}

/**
  \brief Checks if the survey Chunk is valid
  */
bool cwSurveyChunk::IsValid() const {
    return (Stations.size() - 1) == Shots.size() && !Stations.empty() && !Shots.empty();
}

int cwSurveyChunk::StationCount() const {
    return Stations.count();
}

cwStation* cwSurveyChunk::Station(int index) const {
    if(StationIndexCheck(index)) {
        return Stations[index];
    }
    return NULL;
}

int cwSurveyChunk::ShotCount() const {
    return Shots.size();
}

cwShot* cwSurveyChunk::Shot(int index) const {
    if(ShotIndexCheck(index)) {
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
    if(!IsValid() && Stations.size() <= 2 && Shots.size() <= 1) {
        //Make valid
        for(int i = Stations.size(); i < 2; i++) {
            Stations.append(new cwStation());
            emit StationsAdded(i, i);
        }

        if(Shots.size() != 1) {
            Shots.append(new cwShot());
            emit ShotsAdded(0, 0);
        }
        return;
    }


    cwStation* fromStation;
    if(Stations.isEmpty()) {
        fromStation = new cwStation();
    } else {
        fromStation = Stations.last();
        if(!fromStation->IsValid()) {
            return;
        }
    }

    cwStation* toStation = new cwStation();
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
void cwSurveyChunk::AppendShot(cwStation* fromStation, cwStation* toStation, cwShot* shot) {
    //qDebug() << "Trying to add shot";
    if(!CanAddShot(fromStation, toStation, shot)) { return; }

    int index;
    int firstIndex = Stations.size();
    if(Stations.empty()) {
        Stations.append(fromStation);
    }

    index = Shots.size();
    Shots.append(shot);
    shot->setParent(this);
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
    newChunk->Stations.append(new cwStation()); //Add an empty station to the front

    //Copy the points from one chunk to another
    for(int i = stationIndex; i < Stations.size(); i++) {

        //Get the current stations and shots
        cwStation* station = Stations[i];
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
    QList<cwStation*>::iterator stationIter = Stations.begin() + stationIndex;
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

    cwStation* station = new cwStation();
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

    cwStation* station = new cwStation();
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
bool cwSurveyChunk::CanAddShot(cwStation* fromStation, cwStation* toStation, cwShot* shot) {
    return fromStation != NULL && toStation != NULL && shot != NULL &&
            (Stations.empty() || Stations.last() == fromStation);
}

/**
  \brief Returns the two and from stations at shot

  This first find the shot and the get's the to and from station.
  */
QPair<cwStation*, cwStation*> cwSurveyChunk::ToFromStations(const cwShot* shot) const {
    if(!IsValid()) { return QPair<cwStation*, cwStation*>(NULL, NULL); }
    if(shot->parent() != this) { return QPair<cwStation*, cwStation*>(NULL, NULL); }

    for(int i = 0; i < Shots.size(); i++) {
        if(Shots[i] == shot) {
            return QPair<cwStation*, cwStation*>(Stations[i], Stations[i + 1]);
        }
    }

    return QPair<cwStation*, cwStation*>(NULL, NULL);
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
    int shotIndex = Index(stationIndex, shot);

    //Remove them
    Remove(stationIndex, shotIndex);
}

/**
  \brief Checks to see if the model can remove the station with the shot direction
  \return true if it can and false if it can't
  */
bool cwSurveyChunk::CanRemoveStation(int stationIndex, Direction shot) {
    if(StationCount() <= 2) { return false; }
    if(stationIndex < 0 || stationIndex >= StationCount()) { return false; }
    int shotIndex = Index(stationIndex, shot);
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
    int stationIndex = Index(shotIndex, station);

    //Remove them
    Remove(stationIndex, shotIndex);
}

/**
  \brief Checks to see if the model can remove the shot with the station direction
  \return true if it can and false if it can't
  */
bool cwSurveyChunk::CanRemoveShot(int shotIndex, Direction station) {
    if(ShotCount() <= 1) { return false; }
    if(shotIndex < 0 || shotIndex >= ShotCount()) { return false; }
    int stationIndex = Index(shotIndex, station);
    if(stationIndex < 0 || stationIndex >= StationCount()) { return false; }

    return true;
}

/**
  \brief Removes a station and a shot from the chunk

  This does no bounds checking!!!
  */
void cwSurveyChunk::Remove(int stationIndex, int shotIndex) {
    Stations.removeAt(stationIndex);
    emit StationsRemoved(stationIndex, stationIndex);

    Shots.removeAt(shotIndex);
    emit ShotsRemoved(shotIndex, shotIndex);
}

/**
  \brief Helper to the remove functions.
  */
int cwSurveyChunk::Index(int index, Direction direction) {
    switch(direction) {
    case Above:
        return index - 1;
    case Below:
        return index;
    }
    return -1;
}

