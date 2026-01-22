/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyChunk.h"
//#include "cwStationReference.h"
#include "cwShot.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwDebug.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwCompassValidator.h"
#include "cwDistanceValidator.h"
#include "cwClinoValidator.h"
#include "cwTripCalibration.h"
#include "cwMath.h"

//Qt includes
#include <QHash>
#include <QDebug>
#include <QVariant>
#include <QSet>
#include <QRegularExpression>

//Std includes
#include <math.h>

namespace {
double absAngleDifferenceDegrees(double first, double second)
{
    double raw = fmod(fabs(first - second), 360.0);
    if(raw > 180.0) {
        raw = 360.0 - raw;
    }
    return raw;
}

bool isVerticalClinoReading(const cwClinoReading& reading)
{
    if(reading.state() != cwClinoReading::State::Valid) {
        return false;
    }

    bool ok = false;
    const double value = reading.toDouble(&ok);
    if(!ok) {
        return false;
    }

    return qAbs(qAbs(value) - 90.0) < 0.001;
}
}

cwSurveyChunk::cwSurveyChunk(QObject * parent) :
    QObject(parent),
    ErrorModel(new cwErrorModel(this)),
    ParentTrip(nullptr),
    d({QUuid::createUuid(), {}, {}})
{


    //Handle updating chunk calibration indexing when stations are added
    connect(this, &cwSurveyChunk::shotsAdded, this, &cwSurveyChunk::updateCalibrationsNewShots);

    //Handle updating chunk calibration indexing when stations are removde
    connect(this, &cwSurveyChunk::shotsRemoved, this, &cwSurveyChunk::updateCalibrationsRemoveShots);

    connect(this, &cwSurveyChunk::added, this,
            [this](int stationBegin, int stationEnd,int shotBegin, int shotEnd) {
                emit stationsAdded(stationBegin, stationEnd);
                emit shotsAdded(shotBegin, shotEnd);
                emit stationCountChanged();
                emit shotCountChanged();
    });
    connect(this, &cwSurveyChunk::removed, this,
            [this](int stationBegin, int stationEnd,int shotBegin, int shotEnd) {
                emit stationsRemoved(stationBegin, stationEnd);
                emit shotsRemoved(shotBegin, shotEnd);
                emit stationCountChanged();
                emit shotCountChanged();
    });

}

// /**
//   \brief Makes a deap copy of the chunk

//   All station will be copied into new stations, with the same data.
//   */
// cwSurveyChunk::cwSurveyChunk(const cwSurveyChunk& chunk) :
//     QObject(),
//     ErrorModel(new cwErrorModel(this)),
//     ParentTrip(nullptr),
//     d(chunk.d)
// {
//     //Copy all the Calibration
//     for(auto iter = chunk.Calibrations.begin(); iter != chunk.Calibrations.end(); iter++) {
//         Calibrations.insert(iter.key(), new cwTripCalibration(*iter.value()));
//         Calibrations.value(iter.key())->setParent(this);
//     }

//     updateErrors();
// }

cwSurveyChunk::~cwSurveyChunk()
{
}


/**
  \brief Checks if the survey Chunk is valid
  */
bool cwSurveyChunk::isValid() const {
    return (d.stations.size() - 1) == d.shots.size() && !d.stations.empty() && !d.shots.empty();
}

/**
  \brief Gets the parent cave for this chunk
  */
cwCave* cwSurveyChunk::parentCave() const {
    return parentTrip() != nullptr ? parentTrip()->parentCave() : nullptr;
}

/**
 * @brief cwSurveyChunk::addCalibration
 * @param shotIndex
 *
 * Create's a new calibration at shotIndex. If the shotIndex is invalid, this does nothing
 *
 * You can get the newly created calibration by calling calibrations().value(shotIndex). If
 * the shotIndex is invalid, this request should return nullptr. Valid shot index is from 0
 * to shots().size(). If the chunk is empty, there's no valid index.
 *
 * Chunk calibration are used to modify the calibration mid survey. This is useful, if the
 * instrument changes, or the survey tape breaks.
 *
 * Calibrations that already exist at shotIndex will not be overwritten if calibration isn't
 * nullptr. This function will do nothing in this case. If calibration isn't nullptr, this will
 * replace the current calibration. This object will take ownership of the calibration.
 *
 * Any shot (including the shot at shotIndex) and all shots in chunks listed below this chunk
 * in the trip will have this calibration applied to it, unless other chunks have other overriding
 * calibrations. If you want to set the calibration for the whole trip, see calibration in cwTrip.
 */
void cwSurveyChunk::addCalibration(int shotIndex, cwTripCalibration* calibration)
{
    Q_ASSERT(shotIndex >= 0);
    Q_ASSERT(shotIndex <= shotCount());

    if(calibration == nullptr) {
        if(!Calibrations.contains(shotIndex)) {
            Calibrations.insert(shotIndex, new cwTripCalibration(this));
            emit calibrationsChanged();
        }
    } else {
        calibration->setParent(this);
        if(!Calibrations.contains(shotIndex)) {
            Calibrations.insert(shotIndex, calibration);
            emit calibrationsChanged();
        }
    }
}

/**
 * @brief cwSurveyChunk::removeCalibration
 * @param shotIndex
 *
 * Removes the calibration at index. If no calibration exists at shotIndex, this does nothing.
 *
 * This will schedule the calibration for deletion.
 */
void cwSurveyChunk::removeCalibration(int shotIndex)
{
    Q_ASSERT(shotIndex >= 0);
    Q_ASSERT(shotIndex < shotCount());

    if(Calibrations.contains(shotIndex)) {
        Calibrations.value(shotIndex)->deleteLater();
        Calibrations.remove(shotIndex);
        emit calibrationsChanged();
    }
}

/**
 * @brief cwSurveyChunk::calibrations
 * @return Returns the current chunk's calibrations
 *
 * By default, this will return an empty hash. The key in the hash is the shotIndex of
 * the survey chunk.
 */
QMap<int, cwTripCalibration *> cwSurveyChunk::calibrations() const
{
    return Calibrations;
}

/**
 * @brief cwSurveyChunk::lastCalibration
 * @return calibrations().last();
 *
 * If calibrations is empty this returns nullptr
 */
cwTripCalibration *cwSurveyChunk::lastCalibration() const
{
    if(Calibrations.isEmpty()) {
        return nullptr;
    }
    return Calibrations.last();
}

/**
 * @brief cwSurveyChunk::stationCount
 * @return Returns the number of stations in the survey chunk
 */
int cwSurveyChunk::stationCount() const {
    return d.stations.count();
}

/**
  \brief Sets the parent trip for this chunk
  */
void cwSurveyChunk::setParentTrip(cwTrip* trip) {
    if(ParentTrip != trip) {
        if(ParentTrip != nullptr) {
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::frontCompassCalibrationChanged,
                       this, &cwSurveyChunk::updateCompassErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::backCompassCalibrationChanged,
                    this, &cwSurveyChunk::updateCompassErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::frontClinoCalibrationChanged,
                       this, &cwSurveyChunk::updateClinoErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::backClinoCalibrationChanged,
                    this, &cwSurveyChunk::updateClinoErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::correctedCompassBacksightChanged,
                       this, &cwSurveyChunk::updateCompassErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::correctedClinoBacksightChanged,
                       this, &cwSurveyChunk::updateClinoErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::correctedCompassFrontsightChanged,
                       this, &cwSurveyChunk::updateCompassErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::correctedClinoFrontsightChanged,
                       this, &cwSurveyChunk::updateClinoErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::frontSightsChanged,
                       this, &cwSurveyChunk::updateCompassClinoErrors);
            disconnect(ParentTrip->calibrations(), &cwTripCalibration::backSightsChanged,
                       this, &cwSurveyChunk::updateCompassClinoErrors);
        }

        ParentTrip = trip;
        setParent(trip);

        if(ParentTrip != nullptr) {
            connect(ParentTrip->calibrations(), &cwTripCalibration::frontCompassCalibrationChanged,
                       this, &cwSurveyChunk::updateCompassErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::backCompassCalibrationChanged,
                    this, &cwSurveyChunk::updateCompassErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::frontClinoCalibrationChanged,
                       this, &cwSurveyChunk::updateClinoErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::backClinoCalibrationChanged,
                    this, &cwSurveyChunk::updateClinoErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::correctedCompassBacksightChanged,
                       this, &cwSurveyChunk::updateCompassErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::correctedClinoBacksightChanged,
                       this, &cwSurveyChunk::updateClinoErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::correctedCompassFrontsightChanged,
                       this, &cwSurveyChunk::updateCompassErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::correctedClinoFrontsightChanged,
                       this, &cwSurveyChunk::updateClinoErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::frontSightsChanged,
                    this, &cwSurveyChunk::updateCompassClinoErrors);
            connect(ParentTrip->calibrations(), &cwTripCalibration::backSightsChanged,
                       this, &cwSurveyChunk::updateCompassClinoErrors);
        }

        updateCompassClinoErrors();

        emit parentTripChanged();
    }
}

cwStation cwSurveyChunk::station(int index) const {
    if(stationIndexCheck(index)) {
        return d.stations[index];
    }
    return cwStation();
}

int cwSurveyChunk::shotCount() const {
    return d.shots.size();
}

cwShot cwSurveyChunk::shot(int index) const {
    if(shotIndexCheck(index)) {
        return d.shots[index];
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
    if(!isValid() && d.stations.size() <= 2 && d.shots.size() <= 1) {
        //Make valid
        for(int i = d.stations.size(); i < 2; i++) {
            d.stations.append(cwStation());
            // emit stationsAdded(i, i);
        }

        if(d.shots.size() != 1) {
            d.shots.append(cwShot());
            // emit shotsAdded(0, 0);
        }

        //Added initial stations
        emit added(0, 1, 0, 0);

        checkForStationError(d.stations.size() - 2);
        checkForStationError(d.stations.size() - 1);
        checkForShotError(d.shots.size() - 1);

        return;
    }


    cwStation fromStation;
    if(!d.stations.isEmpty()) {
        fromStation = d.stations.last();
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
    int firstIndex = d.stations.size();
    if(d.stations.empty()) {
        d.stations.append(fromStation);
        checkForStationError(d.stations.size() - 1);
    }

    int shotIndex = d.shots.size();
    d.shots.append(shot);

    int stationIndex = d.stations.size();
    d.stations.append(toStation);

    emit added(firstIndex, stationIndex, shotIndex, shotIndex);

    checkForStationError(stationIndex);
    checkForShotError(shotIndex);
}

/**
  \brief Splits the current survey chunk at stationIndex

  This will split the this chunk into two pieces, if it's valid to split there, ie
  the station Index, is valid.

  This will create a new chunk, that the caller is responsible for deleting
  */
cwSurveyChunk* cwSurveyChunk::splitAtStation(int stationIndex) {
    if(stationIndex < 1 || stationIndex >= d.stations.size()) { return nullptr; }

    cwSurveyChunk* newChunk = new cwSurveyChunk(this);
    newChunk->d.stations.append(cwStation()); //Add an empty station to the front

    //Copy the points from one chunk to another
    for(int i = stationIndex; i < d.stations.size(); i++) {

        //Get the current stations and shots
        cwStation station = d.stations[i];
        cwShot currentShot = shot(i - 1);

        newChunk->d.stations.append(station);
        if(currentShot.isValid()) {
            newChunk->d.shots.append(currentShot);
        }
    }

    //Remove the stations and shots from the list
    int shotIndex = stationIndex - 1;
    emit aboutToRemove(stationIndex, stationIndex, shotIndex, shotIndex);

    QList<cwStation>::iterator stationIter = d.stations.begin() + stationIndex;
    QList<cwShot>::iterator shotIter = d.shots.begin() + shotIndex;
    d.stations.erase(stationIter, d.stations.end());
    d.shots.erase(shotIter, d.shots.end());

    emit removed(stationIndex, stationIndex, shotIndex, shotIndex);

    //Check for errors
    updateErrors();

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
    qDebug() << "InsertStation:" << this << stationIndex << direction << d.stations.empty() << (stationIndex < 0) << (stationIndex >= d.stations.size());


    if(d.stations.empty()) { appendNewShot(); return; }
    if(stationIndex < 0 || stationIndex >= d.stations.size()) { return; }

    int shotIndex = stationIndex;

    if(direction == Below) {
        stationIndex++;
    }

    cwStation station;

    d.stations.insert(stationIndex, station);
    d.shots.insert(shotIndex, cwShot());

    emit added(stationIndex, stationIndex,
               shotIndex, shotIndex);

    updateErrors();
}

/**
  Inserts a shot at index stationIndex

  If direction is above, it's inserted at index, if it's below, then this is insert
  at index + 1.  A station will also be added as well
  */
void cwSurveyChunk::insertShot(int shotIndex, Direction direction) {
    if(d.stations.empty()) { appendNewShot(); return; }
    if(shotIndex < 0 || shotIndex >= d.stations.size()) { return; }

    int stationIndex = shotIndex + 1;

    if(direction == Below) {
        shotIndex++;
    }

    cwStation station;

    d.stations.insert(stationIndex, station);
    d.shots.insert(shotIndex, cwShot());

    emit added(stationIndex, stationIndex,
               shotIndex, shotIndex);

    updateErrors();
}


/**
  \brief Checks if a shot can be added to the chunk

  \returns true if AddShot() will be successfull or will do nothing
  */
bool cwSurveyChunk::canAddShot(const cwStation& fromStation, const cwStation& toStation) {
    Q_UNUSED(toStation);
    return d.stations.empty() || d.stations.last().name().compare(fromStation.name(), Qt::CaseInsensitive) == 0;
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

    //Refresh all the errors
    updateErrors();
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
    int stationIndex = index(shotIndex+1, station);

    //Remove them
    remove(stationIndex, shotIndex);

    //Refresh all the errors
    updateErrors();
}

/**
  \brief Checks to see if the model can remove the shot with the station direction
  \return true if it can and false if it can't
  */
bool cwSurveyChunk::canRemoveShot(int shotIndex, Direction station) {
    if(shotCount() <= 1) { return false; }
    if(shotIndex < 0 || shotIndex >= shotCount()) { return false; }
    int stationIndex = index(shotIndex+1, station);
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
    case ShotDistanceIncludedRole:
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
    if(d.stations.size() < 2) {
        return QString();
    }

    if(d.stations.last().name().isEmpty()) {
        QString stationName;

        if(d.stations.size() == 2) {
            //Try to get the station name from the previous chunk
            QList<cwSurveyChunk*> chunks = parentTrip()->chunks();
            int index = chunks.indexOf(const_cast<cwSurveyChunk*>(this)) - 1;
            cwSurveyChunk* previousChunk = parentTrip()->chunk(index);
            if(previousChunk != nullptr) {
                stationName = previousChunk->d.stations.last().name();
            }
        }

        if(stationName.isEmpty()) {
            int secondToLastStation = d.stations.size() - 2;
            stationName = d.stations.at(secondToLastStation).name();
        }

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
    // Look for numbers to increment
    const static QRegularExpression regexp("(\\D*)(\\d+)");
    QRegularExpressionMatch match = regexp.match(stationName);

    if (match.hasMatch()) {
        QString surveyNamePrefix = match.captured(1);
        QString stationNumberString = match.captured(2);

        int stationNumber = stationNumberString.toInt();
        stationNumber++;

        return QString("%1%2").arg(surveyNamePrefix).arg(stationNumber);
    }

    return QString();
}

/**
 * @brief cwSurveyChunk::setStation
 * @param station - The station that'll be changed
 * @param index - The index of the station that'll be changed
 *
 * If the index is out of range, this function will do nothing
 */
void cwSurveyChunk::setStation(cwStation station, int index){
    if(index < 0 || index >= d.stations.size()) { return; }
    d.stations[index] = station;
    emit dataChanged(StationNameRole, index);
    emit dataChanged(StationLeftRole, index);
    emit dataChanged(StationRightRole, index);
    emit dataChanged(StationUpRole, index);
    emit dataChanged(StationDownRole, index);

    checkForStationError(index);
}

/**
 * @brief cwSurveyChunk::isStationAndShotsEmpty
 * @return This return true if all the station and shot data
 * is empty.  If thes chunk has any station or shot data this returns false.
 *
 * The chunk can have shot and station, but if the no data in the shot and
 * stations then this return false.
 */
bool cwSurveyChunk::isStationAndShotsEmpty() const
{
    if(!isValid()) {
        return true;
    }

    foreach(cwStation station, d.stations) {
        if(!station.name().isEmpty() ||
            station.left().state() != cwDistanceReading::State::Empty ||
            station.right().state() != cwDistanceReading::State::Empty ||
            station.up().state() != cwDistanceReading::State::Empty ||
            station.down().state() != cwDistanceReading::State::Empty)
        {
            return false;
        }
    }

    foreach(cwShot shot, d.shots) {
        if(shot.distance().state() != cwDistanceReading::State::Empty ||
                shot.backCompass().state() != cwCompassReading::State::Empty ||
                shot.compass().state() != cwCompassReading::State::Empty ||
                shot.clino().state() != cwClinoReading::State::Empty ||
                shot.backClino().state() != cwClinoReading::State::Empty)
        {
            return false;
        }
    }

    return true;
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

cwSurveyChunkData cwSurveyChunk::data() const
{
    auto data = d;

    data.calibrations.clear();

    for(auto iter = Calibrations.constKeyValueBegin();
         iter != Calibrations.constKeyValueEnd();
         ++iter)
    {
        data.calibrations.insert(iter->first, iter->second->data());
    }

    return data;
}

void cwSurveyChunk::setData(const cwSurveyChunkData &data)
{
    auto lastStationIndex = d.stations.size() - 1;
    auto lastShotIndex = d.shots.size() - 1;

    emit aboutToRemove(0, lastStationIndex, 0, lastShotIndex);
    d.shots.clear();
    d.stations.clear();
    emit removed(0, lastStationIndex, 0, lastShotIndex);

    //Removed all trip calibrations
    for(auto value : std::as_const(Calibrations)) {
        value->deleteLater();
    }
    Calibrations.clear();


    d = data;

    if(d.stations.size() > 0) {
        emit added(0, d.stations.size() - 1, 0, d.shots.size());
    }

    //Add all the calibrations
    for(auto iter = d.calibrations.keyValueBegin();
         iter != d.calibrations.keyValueEnd();
         ++iter)
    {
        auto calibration = new cwTripCalibration(this);
        calibration->setData(iter->second);
        addCalibration(iter->first, calibration);
    }

    updateErrors();
}

/**
  \brief Helper function to data
  */
QVariant cwSurveyChunk::stationData(DataRole role, int index) const {
    if(index < 0 || index >= d.stations.size()) { return QVariant(); }

    const cwStation& station = d.stations[index];

    switch (role) {
    case StationNameRole:
        return station.name();
    case StationLeftRole:
        return QVariant::fromValue(station.left());
        break;
    case StationRightRole:
        return QVariant::fromValue(station.right());
        break;
    case StationUpRole:
        return QVariant::fromValue(station.up());
        break;
    case StationDownRole:
        return QVariant::fromValue(station.down());
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
    if(index < 0 || index >= d.shots.size()) { return QVariant(); }

    const cwShot& shot = d.shots[index];

    switch(role) {
    case ShotDistanceRole:
        return QVariant::fromValue(shot.distance());
    case ShotDistanceIncludedRole:
        return shot.isDistanceIncluded();
    case ShotCompassRole:
        return QVariant::fromValue(shot.compass());
    case ShotBackCompassRole:
        return QVariant::fromValue(shot.backCompass());
    case ShotClinoRole:
        return QVariant::fromValue(shot.clino());
    case ShotBackClinoRole:
        return QVariant::fromValue(shot.backClino());
    default:
        return QVariant();
    }
    return QVariant();
}

/**
  \brief Sets the station's data for role, index, and data
  */
void cwSurveyChunk::setStationData(cwSurveyChunk::DataRole role, int index, const QVariant& data) {
    if(index < 0 || index >= d.stations.size()) {
        qDebug().noquote() << QStringLiteral("Can't set station data for role \"%1\" at index: \"%2\" with data: \"%3\" index out of bounds")
        .arg(role).arg(index).arg(data.toString()) << LOCATION;
        return;
    }

    if(!data.canConvert<QString>()) {
        qDebug() << "Can't convert data to variant" << LOCATION;
        return;
    }

    QString dataString = data.toString();
    cwStation& station = d.stations[index];

    switch (role) {
    case StationNameRole:
        station.setName(dataString);
        emit dataChanged(role, index);
        break;
    case StationLeftRole:
        station.setLeft(dataString);
        emit dataChanged(role, index);
        break;
    case StationRightRole:
        station.setRight(dataString);
        emit dataChanged(role, index);
        break;
    case StationUpRole:
        station.setUp(dataString);
        emit dataChanged(role, index);
        break;
    case StationDownRole:
        station.setDown(dataString);
        emit dataChanged(role, index);
        break;
    default:
        qDebug() << "Can't find role:" << role << LOCATION;
    }

    checkForErrorOnDataChanged(role, index);

}

/**
  \brief Sets the shot's data for role, index, and data
  */
void cwSurveyChunk::setShotData(cwSurveyChunk::DataRole role, int index, const QVariant& data) {
    if(index < 0 || index >= d.shots.size()) {
        qDebug() << QString("Can't set shot data for role \"%1\" at index: \"%2\" with data: \"%3\"")
                    .arg(role).arg(index).arg(data.toString()) << LOCATION;
        return;
    }

    if(!data.canConvert<QString>()) {
        qDebug() << "Can't convert data to variant" << LOCATION;
        return;
    }

    cwShot& shot = d.shots[index];

    switch(role) {
    case ShotDistanceRole:
        shot.setDistance(data.toString());
        emit dataChanged(role, index);
        break;
    case ShotDistanceIncludedRole:
        shot.setDistanceIncluded(data.toBool());
        emit dataChanged(role, index);
        break;
    case ShotCompassRole:
        shot.setCompass(data.toString());
        emit dataChanged(role, index);
        break;
    case ShotBackCompassRole:
        shot.setBackCompass(data.toString());
        emit dataChanged(role, index);
        break;
    case ShotClinoRole:
        shot.setClino(data.toString());
        emit dataChanged(role, index);
        break;
    case ShotBackClinoRole:
        shot.setBackClino(data.toString());
        emit  dataChanged(role, index);
        break;
    default:
        qDebug() << "Can't find role:" << role << LOCATION;
    }

    checkForErrorOnDataChanged(role, index);
}

void cwSurveyChunk::checkForErrorOnDataChanged(cwSurveyChunk::DataRole role, int index)
{
    checkForError(role, index);

    //Check dependent boxes
    switch(role) {
    case StationNameRole: {
        checkForError(StationLeftRole, index);
        checkForError(StationRightRole, index);
        checkForError(StationUpRole, index);
        checkForError(StationDownRole, index);
        if(index - 1 >= 0) {
            checkForError(StationNameRole, index - 1);
        }
        if(index + 1 < d.stations.size()) {
            checkForError(StationNameRole, index + 1);
        }

        //Previous shot
        const int previousIndex = index - 1;
        if(previousIndex >= 0) {
            checkForError(ShotDistanceRole, previousIndex);
            checkForError(ShotCompassRole, previousIndex);
            checkForError(ShotBackCompassRole, previousIndex);
            checkForError(ShotClinoRole, previousIndex);
            checkForError(ShotBackClinoRole, previousIndex);
        }

        //Next shot
        if(index < d.shots.size()) {
            checkForError(ShotDistanceRole, index);
            checkForError(ShotCompassRole, index);
            checkForError(ShotBackCompassRole, index);
            checkForError(ShotClinoRole, index);
            checkForError(ShotBackClinoRole, index);
        }
        break;
    }
    case StationLeftRole:
        //No dependancy
        break;
    case StationRightRole:
        //No dependancy
        break;
    case StationUpRole:
        //No dependancy
        break;
    case StationDownRole:
        //No dependancy
        break;
    case ShotDistanceRole:
        checkForError(StationNameRole, index);
        checkForError(StationNameRole, index + 1);
        break;
    case ShotDistanceIncludedRole:
        //No dependancy
        break;
    case ShotCompassRole:
        checkForError(ShotBackCompassRole, index);
        checkForError(StationNameRole, index);
        checkForError(StationNameRole, index + 1);
        break;
    case ShotBackCompassRole:
        checkForError(ShotCompassRole, index);
        checkForError(StationNameRole, index);
        checkForError(StationNameRole, index + 1);
        break;
    case ShotClinoRole:
        checkForError(ShotBackClinoRole, index);
        checkForError(ShotCompassRole, index);
        checkForError(ShotBackCompassRole, index);
        checkForError(StationNameRole, index);
        checkForError(StationNameRole, index + 1);
        break;
    case ShotBackClinoRole:
        checkForError(ShotClinoRole, index);
        checkForError(ShotCompassRole, index);
        checkForError(ShotBackCompassRole, index);
        checkForError(StationNameRole, index);
        checkForError(StationNameRole, index + 1);
        break;
    default:
        break;
    }
}

/**
 * @brief cwSurveyChunk::checkForError
 * @param role - The role that will be checked
 * @param index - The index o
 */
void cwSurveyChunk::checkForError(cwSurveyChunk::DataRole role, int index)
{
    Q_UNUSED(role)
    Q_UNUSED(index)

    QList<cwError> errors;

    switch(role) {
    case StationNameRole: {

        Q_ASSERT(index >= 0);
        Q_ASSERT(index < d.stations.size());
        const cwStation& station = d.stations.at(index);
        QString stationName = station.name(); //data(StationNameRole, index).toString();

        if(stationName.isEmpty()) {
            if(!isStationAndShotsEmpty()) {

                const int previousShotIndex = index - 1;
                const int nextShotIndex = index;
                bool previousShotHasData = false;
                bool nextShotHasData = false;

                if(previousShotIndex >= 0) {
                    previousShotHasData = !isShotDataEmpty(previousShotIndex);
                }

                if(nextShotIndex < d.shots.size()) {
                    nextShotHasData = !isShotDataEmpty(nextShotIndex);
                }

                if(previousShotHasData || nextShotHasData) {
                    //Station needs to have a name when either adjacent shot has data
                    cwError error;
                    error.setMessage("Missing station name");
                    error.setType(cwError::Fatal);
                    errors.append(error);
                }
            }
        } else {
            const int previousStationIndex = index - 1;
            if(previousStationIndex >= 0) {
                const QString previousStationName = d.stations.at(previousStationIndex).name();
                if(!previousStationName.isEmpty()
                    && previousStationName.compare(stationName, Qt::CaseInsensitive) == 0)
                {
                    cwError error;
                    error.setMessage(QString("Duplicate station name \"%1\"").arg(stationName));
                    error.setType(cwError::Fatal);
                    errors.append(error);
                }
            }
        }

        break;
    }
    case StationLeftRole:
    case StationRightRole:
    case StationUpRole:
    case StationDownRole:
        errors = checkLRUDError(role, index);
        break;
    case ShotDistanceIncludedRole:
        break;
    case ShotDistanceRole:
    case ShotCompassRole:
    case ShotBackCompassRole:
    case ShotClinoRole:
    case ShotBackClinoRole:
        errors = checkDataError(role, index);
        break;
    }

    CellIndex cellIndex = CellIndex(index, role);
    cwErrorModel* cellModel = nullptr;
    if(!CellErrorModels.contains(cellIndex) && !errors.isEmpty()) {
        cellModel = new cwErrorModel(ErrorModel);
        cellModel->setParentModel(ErrorModel);
        CellErrorModels.insert(cellIndex, cellModel);
    } else {
        cellModel = CellErrorModels.value(cellIndex, nullptr);
    }

    if(cellModel != nullptr) {
        if(errors.isEmpty()) {
            //Delete the cellModel and clear it from the database of errors
            cellModel->setParentModel(nullptr);
            cellModel->deleteLater();
            CellErrorModels.remove(cellIndex);
        }  else {
            //Clear the previous errors
            if(cellModel->errors()->toList() != errors) {
                cellModel->errors()->clear();
                cellModel->errors()->append(errors);
                emit errorsChanged(role, index);
            }
        }
    }
}

/**
 * @brief cwSurveyChunk::checkForStationError
 * @param index
 */
void cwSurveyChunk::checkForStationError(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < d.stations.size());
    checkForError(StationNameRole, index);
    checkForError(StationLeftRole, index);
    checkForError(StationRightRole, index);
    checkForError(StationUpRole, index);
    checkForError(StationDownRole, index);
}

/**
 * @brief cwSurveyChunk::checkForShotError
 * @param index
 */
void cwSurveyChunk::checkForShotError(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < d.shots.size());
    checkForError(ShotDistanceRole, index);
    checkForError(ShotDistanceIncludedRole, index);
    checkForError(ShotCompassRole, index);
    checkForError(ShotBackCompassRole, index);
    checkForError(ShotClinoRole, index);
    checkForError(ShotBackClinoRole, index);
}

/**
 * @brief cwSurveyChunk::checkLRUDError
 * @param role
 * @param index
 * @return The list of errors when checking LRUD data
 */
QList<cwError> cwSurveyChunk::checkLRUDError(cwSurveyChunk::DataRole role, int index) const
{
    QList<cwError> errors;

    QString direction;
    switch(role) {
    case StationLeftRole:
        direction = "left";
        break;
    case StationRightRole:
        direction = "right";
        break;
    case StationUpRole:
        direction = "up";
        break;
    case StationDownRole:
        direction = "down";
        break;
    default:
        Q_ASSERT(false); //Should never get here, you're using this function wrong
    }

    QString stationName = data(cwSurveyChunk::StationNameRole, index).toString();

    QVariant value = data(role, index);

    Q_ASSERT(!value.isNull());

    //The LRUD data isn't a number
    Q_ASSERT(value.canConvert<cwDistanceReading>());
    cwDistanceReading reading = value.value<cwDistanceReading>();

    switch(reading.state()) {
    case cwDistanceReading::State::Invalid: {
        cwError error;
        error.setType(cwError::Fatal);
        error.setMessage(QString("The \"%1\" value (\"%2\") isn't a number for station \"%3\"").arg(direction, reading.value(), stationName));
        errors.append(error);
        break;
    }
    case cwDistanceReading::State::Empty: {
        if(!stationName.isEmpty()) {
            cwError error;
            error.setType(cwError::Warning);
            error.setMessage(QString("Missing \"%1\" for station \"%2\"").arg(direction, stationName));
            errors.append(error);
        }
        break;
    }
    case cwDistanceReading::State::Valid:
        break;
    }
    // }

    return errors;
}

/**
 * @brief cwSurveyChunk::checkDataError
 * @param roleName
 * @param variant
 * @param validator
 * @return
 */
QList<cwError> cwSurveyChunk::checkDataError(DataRole role, int index) const
{
    QList<cwError> errors;

    if(parentTrip() != nullptr) {
        if(!parentTrip()->calibrations()->hasFrontSights()) {
            //Front sights are disabled
            switch(role) {
            case ShotCompassRole:
                return errors; //No errors generated fro this role
            case ShotClinoRole:
                return errors; //No errors generated fro this role
            default:
                break;
            }
        }

        if(!parentTrip()->calibrations()->hasBackSights()) {
            //Backsights sights are disabled
            switch(role) {
            case ShotBackCompassRole:
                return errors; //No errors generated fro this role
            case ShotBackClinoRole:
                return errors; //No errors generated fro this role
            default:
                break;
            }
        }
    }

    cwValidator* validatorPtr = nullptr;
    QString roleName;
    QVariant value = data(role, index);
    QVariant backSightValue;

    switch(role) {
    case ShotDistanceRole:
        roleName = "distance";
        validatorPtr = new cwDistanceValidator();
        break;
    case ShotCompassRole:
        roleName = "compass";
        validatorPtr = new cwCompassValidator();
        backSightValue = data(ShotBackCompassRole, index);
        errors.append(checkWithTolerance(ShotCompassRole, ShotBackCompassRole, index));
        break;
    case ShotBackCompassRole:
        roleName = "backsite compass";
        validatorPtr = new cwCompassValidator();
        backSightValue = data(ShotCompassRole, index);
        break;
    case ShotClinoRole:
        roleName = "clino";
        validatorPtr = new cwClinoValidator();
        backSightValue = data(ShotBackClinoRole, index);
        errors.append(checkWithTolerance(ShotClinoRole, ShotBackClinoRole, index));
        break;
    case ShotBackClinoRole:
        roleName = "backsite clino";
        validatorPtr = new cwClinoValidator();
        backSightValue = data(ShotClinoRole, index);
        break;
    default:
        Q_ASSERT(false); //Bug!!!! Shouldn't get herer
        break;
    }

    QString fromStation = data(StationNameRole, index).toString();
    QString toStation = data(StationNameRole, index + 1).toString();

    QScopedPointer<cwValidator> validator(validatorPtr);

    Q_ASSERT(!value.isNull());
    Q_ASSERT(value.canConvert<cwReading>());
    cwReading reading = value.value<cwReading>();

    if(!fromStation.isEmpty() && !toStation.isEmpty()
        && reading.value().isEmpty()
        && !isClinoDownOrUp(role, index))
    {
        //Data should have a value
        cwError error;

        if(role != ShotDistanceRole) {
            Q_ASSERT(!backSightValue.isNull());
            Q_ASSERT(backSightValue.canConvert<cwReading>());
            cwReading backSightReading = backSightValue.value<cwReading>();

            if(backSightReading.value().isEmpty()) {
                error.setType(cwError::Fatal);
            } else {
                error.setType(cwError::Warning);
            }
        } else {
            //Missing data in the distance role
            error.setType(cwError::Fatal);
        }

        error.setMessage(QString("Missing \"%1\" from shot \"%2\" ➔ \"%3\"").arg(roleName, fromStation, toStation));
        errors.append(error);
    } else if(!value.isNull()) {
        Q_ASSERT(value.canConvert<cwReading>());
        cwReading reading = value.value<cwReading>();
        int pos = 0;
        QString valueString = reading.value();
        QValidator::State state = validator->validate(valueString, pos);
        if(state != QValidator::Acceptable) {
            //Data in the field is bad...
            cwError error;
            error.setMessage(validator->errorText());
            error.setType(cwError::Fatal);
            errors.append(error);
        }

        errors.append(checkClinoMixingType(role, index));
    }

    return errors;
}

/**
 * @brief cwSurveyChunk::checkWithTolerance
 * @param frontSightRole - frontsite role
 * @param backSightRole - backsite role
 * @param index - The index
 * @param tolerance - The tolerance of the errro
 * @param units - For formatting the error message
 * @return Returns errors if the back and frontsite don't agree within tolerance
 */
QList<cwError> cwSurveyChunk::checkWithTolerance(cwSurveyChunk::DataRole frontSightRole,
                                                 cwSurveyChunk::DataRole backSightRole,
                                                 int index,
                                                 double tolerance,
                                                 QString units) const
{
    QList<cwError> errors;

    bool okay;
    double frontSight = data(frontSightRole, index).toDouble(&okay);
    if(!okay) { return errors; }

    double backSight = data(backSightRole, index).toDouble(&okay);
    if(!okay) { return errors; }

    //Correct the backSight
    switch(frontSightRole) {
    case ShotCompassRole:
        if(parentTrip() != nullptr) {
            frontSight += parentTrip()->calibrations()->frontCompassCalibration();
            backSight += parentTrip()->calibrations()->backCompassCalibration();

            if(parentTrip()->calibrations()->hasCorrectedCompassFrontsight()) {
                frontSight += 180.0;
            }

            if(parentTrip()->calibrations()->hasCorrectedCompassBacksight()) {
                backSight += 180.0;
            }

        }
        //Flip the backsight so it's in frontsight space
        frontSight = cwWrapDegrees360(frontSight);
        backSight = cwWrapDegrees360(backSight + 180.0);
        break;
    case ShotClinoRole:
        if(parentTrip() != nullptr) {
            frontSight += parentTrip()->calibrations()->frontClinoCalibration();
            backSight += parentTrip()->calibrations()->backClinoCalibration();

            if(parentTrip()->calibrations()->hasCorrectedClinoFrontsight()) {
                frontSight *= -1;
            }

            if(parentTrip()->calibrations()->hasCorrectedClinoBacksight()) {
                backSight *= -1;
            }
        }

        //Flip the backsight so it's in frontsight space
        backSight *= -1;

        frontSight = qBound(-90.0, frontSight, 90.0);
        backSight = qBound(-90.0, backSight, 90.0);

        break;
    default:
        Q_ASSERT(false);
        break;
    }

    double differance = 0.0;
    if(frontSightRole == ShotCompassRole) {
        differance = absAngleDifferenceDegrees(frontSight, backSight);
    } else {
        differance = fabs(frontSight - backSight);
    }
    if(differance > tolerance) {
        cwError error;
        error.setMessage(QString("Frontsight and backsight differs by %1%2").arg(differance).arg(units));
        error.setType(cwError::Warning);
        errors.append(error);
    }

    return errors;
}

/**
 * @brief cwSurveyChunk::checkMixingType
 * @param role
 * @param index
 * @return The fatal error if the clino reading mix type. Mix type occures when using Up|Down and
 * number for front and backsight of the clino. We need only Up and Down or Numbers for clino, not
 * both at the same time. This always return an empty list if role isn't a ShotClinoRole
 */
QList<cwError> cwSurveyChunk::checkClinoMixingType(cwSurveyChunk::DataRole role, int index) const
{
    QList<cwError> errors;

    if(parentTrip() != nullptr &&
            !(parentTrip()->calibrations()->hasFrontSights() &&
              parentTrip()->calibrations()->hasBackSights()))
    {
        //Doesn't have backsight or front sight
        return QList<cwError>();
    }

    //Only ShotClinoRole
    if(role == ShotClinoRole) {
        QString clino = data(ShotClinoRole, index).toString();
        QString backClino = data(ShotBackClinoRole, index).toString();

        if(!clino.isEmpty() && !backClino.isEmpty()) {

            bool clinoOkay;
            bool backClinoOkay;

            clino.toDouble(&clinoOkay);
            backClino.toDouble(&backClinoOkay);

            if(!((clinoOkay && backClinoOkay) ||
                 (isClinoDownOrUpHelper(ShotClinoRole, index) && isClinoDownOrUpHelper(ShotBackClinoRole, index))))
            {
                //Mixing types
                cwError error;
                error.setType(cwError::Fatal);
                error.setMessage(QString("You are mixing types. Frontsight and backsight must both be Up or Down, or both numbers"));
                errors.append(error);
            }
        }
    }

    return errors;
}

/**
 * @brief cwSurveyChunk::shotDataEmpty
 * @param index
 * @return True if all the shot data's empty at index, otherwise false
 */
bool cwSurveyChunk::isShotDataEmpty(int index) const
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < d.shots.size());
    const auto& shot = d.shots.at(index);

    return shot.distance().state() == cwDistanceReading::State::Empty
           && shot.compass().state() == cwCompassReading::State::Empty
           && shot.backCompass().state() == cwCompassReading::State::Empty
           && shot.clino().state() == cwClinoReading::State::Empty
           && shot.backClino().state() == cwClinoReading::State::Empty;
}

/**
 * @brief cwSurveyChunk::stationDataEmpty
 * @param index
 * @return True if all the station's data is empty, otherwise false. This excludes the station name
 */
bool cwSurveyChunk::isStationDataEmpty(int index) const
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < d.stations.size());
    const auto& station = d.stations.at(index);

    return station.left().state() == cwDistanceReading::State::Empty
           &&  station.right().state() == cwDistanceReading::State::Empty
           && station.up().state() == cwDistanceReading::State::Empty
           && station.down().state() == cwDistanceReading::State::Empty;
}


/**
 * @brief cwSurveyChunk::updateErrorIndexes
 */
void cwSurveyChunk::updateErrors()
{
    clearErrors();

    for(int i = 0; i < d.shots.size(); i++) {
        checkForShotError(i);
    }

    for(int i = 0; i < d.stations.size(); i++) {
        checkForStationError(i);
    }
}

/**
 * @brief cwSurveyChunk::clinoHasDownOrUp
 * @param role
 * @param index
 *
 * This function only works if role is ShotCompassRole, ShotBackCompassRole, ShotClinoRole or ShotBackClinoRole.
 * Otherwise it always return false. This checks the clino readings to see if a down or up exists.
 * This helps prevent warnings and errors when using Down or Up keywords
 */
bool cwSurveyChunk::isClinoDownOrUp(cwSurveyChunk::DataRole role, int index) const
{
    switch(role) {
    case ShotCompassRole:
    case ShotClinoRole:
    case ShotBackCompassRole:
    case ShotBackClinoRole:
        break;
    default:
        return false;
    }

    if(isClinoDownOrUpHelper(ShotClinoRole, index) || isClinoDownOrUpHelper(ShotBackClinoRole, index)) {
        return true;
    }

    const QVariant frontVariant = data(ShotClinoRole, index);
    const QVariant backVariant = data(ShotBackClinoRole, index);
    Q_ASSERT(frontVariant.canConvert<cwClinoReading>());
    Q_ASSERT(backVariant.canConvert<cwClinoReading>());

    return isVerticalClinoReading(frontVariant.value<cwClinoReading>())
        || isVerticalClinoReading(backVariant.value<cwClinoReading>());
}

/**
 * @brief cwSurveyChunk::isClinoDownOrUpHelper
 * @param role
 * @param index
 * @return
 */
bool cwSurveyChunk::isClinoDownOrUpHelper(cwSurveyChunk::DataRole role, int index) const
{
    const QVariant variant = data(role, index);
    Q_ASSERT(variant.canConvert<cwClinoReading>());
    const cwClinoReading value = variant.value<cwClinoReading>();
    return value.state() == cwClinoReading::State::Up || value.state() == cwClinoReading::State::Down;
}

/**
 * @brief cwSurveyChunk::updateCompassErrors
 *
 * This is called when the calibration, using correct compass change, this is used to update
 * the off by 2 degree errors
 */
void cwSurveyChunk::updateCompassErrors()
{
   for(int i = 0; i < shotCount(); i++) {
       checkForError(ShotCompassRole, i);
       checkForError(ShotBackCompassRole, i);
   }
}

/**
 * @brief cwSurveyChunk::updateClinoErrors
 *
 * This is called when the calibration, using correct compass change, this is used to update
 * the off by 2 degree errors
 */
void cwSurveyChunk::updateClinoErrors()
{
    for(int i = 0; i < shotCount(); i++) {
        checkForError(ShotClinoRole, i);
        checkForError(ShotBackClinoRole, i);
    }
}

/**
 * @brief cwSurveyChunk::updateCompassClinoErrors
 */
void cwSurveyChunk::updateCompassClinoErrors()
{
   for(int i = 0; i < shotCount(); i++) {
       checkForError(ShotCompassRole, i);
       checkForError(ShotBackCompassRole, i);
       checkForError(ShotClinoRole, i);
       checkForError(ShotBackClinoRole, i);
   }
}

/**
 * @brief cwSurveyChunk::updateCalibrationsNewShots
 * @param beginIndex
 * @param endIndex
 *
 * Called when ever shots are added to the chunk. This updates the indexes in the
 * Calibrations in the chunk. Usually there's no calibrations to update.
 */
void cwSurveyChunk::updateCalibrationsNewShots(int beginIndex, int endIndex)
{
    if(!Calibrations.isEmpty()) {
        int distance = endIndex - beginIndex + 1;
        QMap<int, cwTripCalibration*> newCalibration;
        bool updated = false;
        for(auto iter = Calibrations.begin(); iter != Calibrations.end(); iter++) {
            //if the calibration at shot x is greater than or equal to the first
            //index that was added and the beginIndex isn't the last shot in shots
            if(beginIndex <= iter.key() && beginIndex != d.shots.size() - distance) {
                //Update the key and shift the calibration down
                Q_ASSERT(iter.value() != nullptr);
                newCalibration.insert(iter.key() + distance, iter.value());
                updated = true;
            } else {
                Q_ASSERT(iter.value() != nullptr);
                newCalibration.insert(iter.key(), iter.value());
            }
        }

        if(updated) {
            Calibrations = newCalibration;
            emit calibrationsChanged();
        }
    }
}

/**
 * @brief cwSurveyChunk::updateCalibrationsRemoveShots
 * @param beginIndex
 * @param endIndex
 *
 * Called when ever shots are added to the chunk. This updates the indexes in the
 * Calibrations in the chunk. Usually there's no calibrations to update.
 */
void cwSurveyChunk::updateCalibrationsRemoveShots(int beginIndex, int endIndex)
{
    if(!Calibrations.isEmpty()) {
        int distance = endIndex - beginIndex + 1;
        QMap<int, cwTripCalibration*> newCalibration;
        bool updated = false;

        for(auto iter = Calibrations.begin(); iter != Calibrations.end(); iter++) {
            int index = iter.key();

            if(endIndex < iter.key()) {
                int newIndex = index - distance;

                //Update the key and shift the calibration down
                Q_ASSERT(newIndex >= 0);
                if(!newCalibration.contains(newIndex)) {
                    newCalibration.insert(newIndex, iter.value());
                    updated = true;
                }
            } else if(beginIndex <= iter.key() && endIndex >= iter.key()) {
                //Deleting the index
                if(beginIndex < d.shots.size()) {
                    //There's a valid shot at beginIndex
                    if(!newCalibration.contains(beginIndex)) {
                        newCalibration.insert(beginIndex, iter.value());
                        updated = true;
                    } else {
                        iter.value()->deleteLater();
                    }
                } else {
                    //Delete the calibration
                    iter.value()->deleteLater();
                    updated = true;
                }
            } else {
                newCalibration.insert(iter.key(), iter.value());
            }
        }

        if(updated) {
            Calibrations = newCalibration;
            emit calibrationsChanged();
        }
    }
}

/**
 * @brief cwSurveyChunk::clearErrors
 * Clears the error list for the chunk, and goes through and updates all the errors
 *
 * If the parentTrip and cave isn't set, this does nothing
 */
void cwSurveyChunk::clearErrors()
{
    ErrorModel->errors()->clear();
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
 * @brief cwSurveyChunk::error
 * @param role
 * @param index
 * @return Returns the error at index with role.
 */
cwErrorModel* cwSurveyChunk::errorsAt(int index, cwSurveyChunk::DataRole role) const
{
    return CellErrorModels.value(CellIndex(index, role), nullptr);
}

/**
  \brief Removes a station and a shot from the chunk

  This does no bounds checking!!!
  */
void cwSurveyChunk::remove(int stationIndex, int shotIndex) {
    emit aboutToRemove(stationIndex, stationIndex, shotIndex, shotIndex);

    d.stations.removeAt(stationIndex);
    d.shots.removeAt(shotIndex);

    emit removed(stationIndex, stationIndex, shotIndex, shotIndex);
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
  \brief Returns true if the survey chunk has a station
  */
bool cwSurveyChunk::hasStation(QString stationName) const {

    //Linear search...
    foreach(cwStation station, d.stations) {
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
    for(int i = 0; i < d.stations.size(); i++) {
        if(d.stations[i].name().compare(stationName, Qt::CaseInsensitive) == 0) {
            indices.append(i);
        }
    }
    return indices;
}
