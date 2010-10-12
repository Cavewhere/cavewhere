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
  \brief Checks if the survey Chunk is valid
  */
bool cwSurveyChunk::IsValid() {
    return (Stations.size() - 1) == Shots.size() && !Stations.empty();
}

int cwSurveyChunk::StationCount() {
    return Stations.count();
}

cwStation* cwSurveyChunk::Station(int index) {
    if(StationIndexCheck(index)) {
        return Stations[index];
    }
    return NULL;
}

int cwSurveyChunk::ShotCount() {
    return Shots.size();
}

cwShot* cwSurveyChunk::Shot(int index) {
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
void cwSurveyChunk::AddNewShot() {
    cwStation* fromStation;
    if(Stations.empty()) {
        fromStation = new cwStation();
    } else {
        fromStation = Stations.last();
        if(!fromStation->IsValid()) {
            return;
        }
    }

    cwStation* toStation = new cwStation();
    cwShot* shot = new cwShot();

    AddShot(fromStation, toStation, shot);
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
void cwSurveyChunk::AddShot(cwStation* fromStation, cwStation* toStation, cwShot* shot) {
    qDebug() << "Trying to add shot";
    if(!CanAddShot(fromStation, toStation, shot)) { return; }

    if(Stations.empty()) {
        Stations.push_back(fromStation);
        emit StationAdded();
    }

    Shots.push_back(shot);
    emit ShotAdded();

    Stations.push_back(toStation);
    emit StationAdded();

    qDebug() << "Adding shot";
}

/**
  \brief Checks if a shot can be added to the chunk

  \returns true if AddShot() will be successfull or will do nothing
  */
bool cwSurveyChunk::CanAddShot(cwStation* fromStation, cwStation* toStation, cwShot* shot) {
    return fromStation != NULL && toStation != NULL && shot != NULL &&
            (Stations.empty() || Stations.last() == fromStation);
}
