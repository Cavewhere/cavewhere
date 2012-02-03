//Our includes
#include "cwSurveyChunk.h"
//#include "cwStationReference.h"
#include "cwShot.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwDebug.h"

//Qt includes
#include <QHash>
#include <QDebug>
#include <QVariant>
#include <QSet>
#include <QRegExp>

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
    Shots = chunk.Shots;
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
//        updateStationsWithNewCave();
        emit parentTripChanged();
    }
}

cwStation cwSurveyChunk::station(int index) const {
    if(stationIndexCheck(index)) {
        return Stations[index];
    }
    return cwStation();
}

int cwSurveyChunk::shotCount() const {
    return Shots.size();
}

cwShot cwSurveyChunk::shot(int index) const {
    if(shotIndexCheck(index)) {
        return Shots[index];
    }
    return cwShot();
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
            Stations.append(cwStation());
            emit stationsAdded(i, i);
        }

        if(Shots.size() != 1) {
            Shots.append(cwShot());
            emit shotsAdded(0, 0);
        }
        return;
    }


    cwStation fromStation;
    if(!Stations.isEmpty()) {
        fromStation = Stations.last();
        if(!fromStation.isValid()) {
            return;
        }
    }

    cwStation toStation;
    appendShot(fromStation, toStation, cwShot());
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
void cwSurveyChunk::appendShot(cwStation fromStation, cwStation toStation, cwShot shot) {
    //qDebug() << "Trying to add shot";
    if(!canAddShot(fromStation, toStation)) { return; }

    int index;
    int firstIndex = Stations.size();
    if(Stations.empty()) {
        Stations.append(fromStation);
    }

    index = Shots.size();
    Shots.append(shot);
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
    newChunk->Stations.append(cwStation()); //Add an empty station to the front

    //Copy the points from one chunk to another
    for(int i = stationIndex; i < Stations.size(); i++) {

        //Get the current stations and shots
        cwStation station = Stations[i];
        cwShot currentShot = shot(i - 1);

        newChunk->Stations.append(station);
        if(currentShot.isValid()) {
            newChunk->Shots.append(currentShot);
        }
    }

    int stationEnd = Stations.size() - 1;
    int shotEnd = Shots.size() - 1;


    //Remove the stations and shots from the list
    int shotIndex = stationIndex - 1;
    QList<cwStation>::iterator stationIter = Stations.begin() + stationIndex;
    QList<cwShot>::iterator shotIter = Shots.begin() + shotIndex;
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

    cwStation station;

    Stations.insert(stationIndex, station);
    Shots.insert(shotIndex, cwShot());

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

    cwStation station;

    Stations.insert(stationIndex, station);
    emit stationsAdded(stationIndex, stationIndex);

    Shots.insert(shotIndex, cwShot());
    emit shotsAdded(shotIndex, shotIndex);
}


/**
  \brief Checks if a shot can be added to the chunk

  \returns true if AddShot() will be successfull or will do nothing
  */
bool cwSurveyChunk::canAddShot(const cwStation& fromStation, const cwStation& toStation) {
    Q_UNUSED(toStation);
    return Stations.empty() || Stations.last().name().compare(fromStation.name(), Qt::CaseInsensitive) == 0;
}

///**
//  \brief Returns the two and from stations at shot

//  This first find the shot and the get's the to and from station.
//  */
//QPair<cwStationReference, cwStationReference> cwSurveyChunk::toFromStations(const cwShot &shot) const {
//    if(!isValid()) { return QPair<cwStationReference, cwStationReference>(); }
//    //if(shot->parent() != this) { return QPair<cwStationReference*, cwStationReference*>(NULL, NULL); }

//    for(int i = 0; i < Shots.size(); i++) {
//        if(Shots[i].sameIntervalPointer(shot)) {
//            return QPair<cwStationReference, cwStationReference>(Stations[i], Stations[i + 1]);
//        }
//    }

//    return QPair<cwStationReference, cwStationReference>();
//}

///**
//  \brief Returns the to and from stations at shot

//  This first find the shot and the get's the to  station.
//  */
//cwStationReference cwSurveyChunk::toStation(const cwShot &shot) const
//{
//    return toFromStations(shot).second;
//}

///**
//  \brief Returns the from stations at shot

//  This first find the shot and the get's the  from station.
//  */
//cwStationReference cwSurveyChunk::fromStation(const cwShot &shot) const
//{
//    return toFromStations(shot).first;
//}

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
  Returns true if role will modify or read station data
  */
bool cwSurveyChunk::isStationRole(cwSurveyChunk::DataRole role) const {
    switch (role) {
    case StationNameRole:
    case StationLeftRole:
    case StationRightRole:
    case StationUpRole:
    case StationDownRole:
        return true;
    default:
        return false;
    }
}

/**
  Returns true if the role will modify or read shot data
  */
bool cwSurveyChunk::isShotRole(cwSurveyChunk::DataRole role) const {
    switch(role) {
    case ShotDistanceRole:
    case ShotCompassRole:
    case ShotBackCompassRole:
    case ShotClinoRole:
    case ShotBackClinoRole:
        return true;
    default:
        return false;
    }
}

/**
    This returns what next station should be.  This will first check
    if the last station is empty.  If the last station isn't empty then
    this function will return an empty string.
  */
QString cwSurveyChunk::guessLastStationName() const {
    //Need a least two stations for this to work.
    if(stations().size() < 2) {
        return QString();
    }

    if(stations().last().name().isEmpty()) {
        int secondToLastStation = stations().size() - 2;
        QString stationName = stations().at(secondToLastStation).name();
        QString nextStation = guessNextStation(stationName);
        return nextStation;
    }

    return QString();
}

/**
  Guess the next station name by a station name.  The station name
  must be any set of letter and the numbers.  Such as A23 would work, but 32A
  wouldn't work. Stations that don't work, will return an empty string.
  */
QString cwSurveyChunk::guessNextStation(QString stationName) const {
    //Look for numbers to increament
    QRegExp regexp("(\\D*)(\\d+)");
    if(regexp.exactMatch(stationName)) {
        QString surveyNamePrefix = regexp.cap(1);
        QString stationNumberString = regexp.cap(2);

        int stationNumber = stationNumberString.toInt();
        stationNumber++;

        return QString("%1%2").arg(surveyNamePrefix).arg(stationNumber);
    }

    return QString();
}

/**
  \brief Get's the chunk data based on a role
  */
QVariant cwSurveyChunk::data(DataRole role, int index) const {
    if(isStationRole(role)) {
        return stationData(role, index);
    } else if(isShotRole(role)) {
        return shotData(role, index);
    } else {
        return QVariant();
    }
}

/**
  \brief Helper function to data
  */
QVariant cwSurveyChunk::stationData(DataRole role, int index) const {
    if(index < 0 || index >= Stations.size()) { return QVariant(); }

    const cwStation& station = Stations[index];

    switch (role) {
    case StationNameRole:
        return station.name();
    case StationLeftRole:
        if(station.leftInputState() == cwDistanceStates::Valid) {
            return station.left();
        }
        break;
    case StationRightRole:
        if(station.rightInputState() == cwDistanceStates::Valid) {
            return station.right();
        }
        break;
    case StationUpRole:
        if(station.upInputState() == cwDistanceStates::Valid) {
            return station.up();
        }
        break;
    case StationDownRole:
        if(station.downInputState() == cwDistanceStates::Valid) {
            return station.down();
        }
        break;
    default:
        return QVariant();
    }
    return QVariant();
}

/**
  \brief Helper function to data
  */
QVariant cwSurveyChunk::shotData(DataRole role, int index) const {
    if(index < 0 || index >= Shots.size()) { return QVariant(); }

    const cwShot& shot = Shots[index];

    switch(role) {
    case ShotDistanceRole:
        if(shot.distanceState() == cwDistanceStates::Valid) {
            return shot.distance();
        }
        break;
    case ShotCompassRole:
        if(shot.compassState() == cwCompassStates::Valid) {
            return shot.compass();
        }
        break;
    case ShotBackCompassRole:
        if(shot.backCompassState() == cwCompassStates::Valid) {
            return shot.backCompass();
        }
    case ShotClinoRole: {
        switch(shot.clinoState()) {
        case cwClinoStates::Valid:
            return shot.clino();
            case cwClinoStates::Empty:
            return QVariant();
        case cwClinoStates::Down:
            return "Down";
        case cwClinoStates::Up:
            return "Up";
        }
        break;
    }
    case ShotBackClinoRole:
        switch(shot.backClinoState()) {
        case cwClinoStates::Valid:
            return shot.backClino();
            case cwClinoStates::Empty:
            return QVariant();
        case cwClinoStates::Down:
            return "Down";
        case cwClinoStates::Up:
            return "Up";
        }
        break;
    default:
        return QVariant();
    }
    return QVariant();
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
    cwStation& station = Stations[index];

    switch (role) {
    case StationNameRole:
        station.setName(dataString);
        dataChanged(role, index);
        break;
    case StationLeftRole:
        station.setLeft(dataString);
        dataChanged(role, index);
        break;
    case StationRightRole:
        station.setRight(dataString);
        dataChanged(role, index);
        break;
    case StationUpRole:
        station.setUp(dataString);
        dataChanged(role, index);
        break;
    case StationDownRole:
        station.setDown(dataString);
        dataChanged(role, index);
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
    cwShot& shot = Shots[index];

    switch(role) {
    case ShotDistanceRole:
        shot.setDistance(dataString);
        dataChanged(role, index);
        break;
    case ShotCompassRole:
        shot.setCompass(dataString);
        dataChanged(role, index);
        break;
    case ShotBackCompassRole:
        shot.setBackCompass(dataString);
        dataChanged(role, index);
        break;
    case ShotClinoRole:
        shot.setClino(dataString);
        dataChanged(role, index);
        break;
    case ShotBackClinoRole:
        shot.setBackClino(dataString);
        dataChanged(role, index);
        break;
    default:
        qDebug() << "Can't find role:" << role << LOCATION;
    }
}


/**
  \brief Set's the chunk data based on a role
  */
void cwSurveyChunk::setData(DataRole role, int index, QVariant data) {
    if(isStationRole(role)) {
        setStationData(role, index, data);
    } else if(isShotRole(role)) {
        setShotData(role, index, data);
    } else {
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

///**
//  Uses the parent trip and updates all the stations in this chunk
//  with the new cave.

//  If the parent trip is null or parent trip's cave is null, this
//  does nothing
//  */
//void cwSurveyChunk::updateStationsWithNewCave() {
//    if(ParentTrip == NULL || ParentTrip->parentCave() == NULL) { return; }

//    for(int i = 0; i < Stations.size(); i++) {
//        cwCave* cave = ParentTrip->parentCave();
//        Stations[i].setCave(cave);
//    }
//}

/**
  \brief This creates a new station in the chunk

  The station will be owned by this chunk, and the parent cave will be set
  for the station
  */
//cwStationReference cwSurveyChunk::createNewStation() {
//    //Create a new station
//    cwStationReference station; // = new cwStationReference();
//    station.setCave(parentCave());
//    return station;
//}

/**
  \brief Returns true if the survey chunk has a station
  */
bool cwSurveyChunk::hasStation(QString stationName) const {

    //Linear search...
    foreach(cwStation station, Stations) {
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
QSet<cwStation> cwSurveyChunk::neighboringStations(QString stationName) const {
    if(!hasStation(stationName)) {
        return QSet<cwStation>();
    }

    //Get all the indices of the station
    QList<int> stationIndices = indicesOfStation(stationName);

    //Create a unique set of neighboring stations
    QSet<cwStation> neighbors;
    foreach(int index, stationIndices) {
        cwStation previousShot = station(index - 1);
        cwStation nextShot = station(index + 1);

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

/**
Sets editting

If this is set to true, the survey chunk will try to keep and empty shot and station a the end of
the chunk.  The station and shot are automatically added, when true.  Once set to false, the survey chunk
will clamp any extra empty stations and shots data off the end.  This allows the user to add station while editting.

This is an interaction thing.
*/
void cwSurveyChunk::setEditting(bool editting) {
    if(Editting != editting) {
        Editting = editting;
        emit edittingChanged();
    }
}


