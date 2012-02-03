//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwSurveyChunk.h"
#include "cwStationReference.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyNoteModel.h"

//Qt includes
#include <QMap>

cwTrip::cwTrip(QObject *parent) :
    QObject(parent),
    ParentCave(NULL)
{
//    DistanceUnit = cwUnits::Meters;
    Team = new cwTeam(this);
    Calibration = new cwTripCalibration(this);
    Date = QDate::currentDate();
    Notes = new cwSurveyNoteModel(this);

    Notes->setParentTrip(this);
}

void cwTrip::Copy(const cwTrip& object)
{
    ParentCave = NULL;

//    DistanceUnit = cwUnits::Meters;

    //Copy the name of the trip
    setName(object.Name);
    setDate(object.Date);

    //Copy the team
    Team = new cwTeam(*(object.Team));
    Team->setParent(this);

    //Copy the calibration
    Calibration = new cwTripCalibration(*(object.Calibration));
    Calibration->setParent(this);

    //Copy the notes model
    Notes = new cwSurveyNoteModel(*(object.Notes));
    Notes->setParentTrip(this);

    //Remove all the originals
    int lastChunkIndex = Chunks.size() - 1;
    Chunks.clear();
    emit chunksRemoved(0, lastChunkIndex);

    //Copy the chunks
    Chunks.reserve(object.Chunks.size());
    for(int i = 0; i < object.Chunks.size(); i++) {
        cwSurveyChunk* objectsChunk = object.Chunks[i];
        cwSurveyChunk* newChunk = new cwSurveyChunk(*objectsChunk);
        newChunk->setParent(this);
        newChunk->setParentTrip(this);
        Chunks.append(newChunk);
    }
    emit chunksInserted(0, object.Chunks.size() - 1);

}

/**
  \brief Copy constructor
  */
cwTrip::cwTrip(const cwTrip& object)
    : QObject(NULL), cwUndoer()
{
    Copy(object);
}

/**
  \brief Assignment operator
  */
cwTrip& cwTrip::operator=(const cwTrip& object) {
    if(&object == this) { return *this; }
    Copy(object);
    return *this;
}

/**
  \brief Set's the name of the survey trip
  */
void cwTrip::setName(QString name) {
    if(name != Name) {
        pushUndo(new NameCommand(this, name));
    }
}

/**
  Sets the date of the trip
  */
void cwTrip::setDate(QDate date) {
    if(date != Date) {
        pushUndo(new DateCommand(this, date));
    }
}

/**
  Sets the team for the trip
  */
void cwTrip::setTeam(cwTeam* team) {
    if(team != Team) {
        if(Team != NULL)  {
            Team->deleteLater(); //Delete the old team
        }

        Team = team;

        if(Team != NULL) {
            Team->setParent(this);
        }

        emit teamChanged();
    }
}

/**
  Sets the calibration for the trip
  */
void cwTrip::setCalibration(cwTripCalibration* calibration) {
    if(calibration != Calibration) {
        Calibration->deleteLater();
        Calibration = calibration;
        Calibration->setParent(this);
        emit calibrationChanged();
    }
}

///**
//  Sets the distance unit for the trip
//  */
//void cwTrip::setDistanceUnit(cwUnits::LengthUnit newDistanceUnit) {
//    if(DistanceUnit != newDistanceUnit) {
//        DistanceUnit = newDistanceUnit;
//        emit distanceUnitChanged(DistanceUnit);
//    }
//}

/**
  \brief Remove the chunks
  */
void cwTrip::removeChunks(int begin, int end) {
    if(end < begin) { return; }

    //Clamp the end and the beginning
    begin = qMax(0, begin);
    end = qMin(end, numberOfChunks());

    for(int i = end; i >= begin; i--) {
        Chunks.at(i)->deleteLater();
        Chunks.removeAt(i);
    }

    emit chunksRemoved(begin, end);
    emit numberOfChunksChanged();
}

/**
  \brief Insert the chunk at row

  The chunk must be valid
  */
void cwTrip::insertChunk(int row, cwSurveyChunk* chunk) {
    if(chunk == NULL) { return; }
    if(row < 0) { row = 0; }
    if(row > Chunks.size()) { row = Chunks.size(); }

    //Make this own the chunk
    chunk->setParentTrip(this);

    Chunks.insert(row, chunk);

    emit chunksInserted(row, row);
    emit numberOfChunksChanged();
}

/**
  \brief Adds the chunk to the trip
  */
 void cwTrip::addChunk(cwSurveyChunk* chunk) {
     insertChunk(numberOfChunks(), chunk);
 }

 /**
    This creates a new chunk that has one shot and two stations.

    This will append the chunk to the end of the chunk list
   */
 void cwTrip::addNewChunk() {
     cwSurveyChunk* chunk = new cwSurveyChunk(this);
     chunk->appendNewShot(); //Creates a valid chunk
     addChunk(chunk);
 }

/**
  \brief Set chunks for the group

  This will reparent all the chunks to this object
  */
void cwTrip::setChucks(QList<cwSurveyChunk*> chunks) {
    //Remove old chunks
    removeChunks(0, numberOfChunks() - 1);

    Chunks = chunks;

    foreach(cwSurveyChunk* chunk, Chunks) {
        chunk->setParentTrip(this);
    }

    emit chunksInserted(0, numberOfChunks() - 1);
}

/**
  \brief Get's the chunk at i

  If i is out of bounds, this returns a null chunk
  */
cwSurveyChunk* cwTrip::chunk(int i) const {
    if(i < 0 || i > numberOfChunks()) {
        return NULL;
    }
    return Chunks[i];
}

/**
  \brief Gets all the chunks
  */
QList<cwSurveyChunk*> cwTrip::chunks() const {
    return Chunks;
}

/**
  \brief Gets the number of stations in the trip
  */
int cwTrip::numberOfStations() const {
    int stationCount = 0;
    foreach(cwSurveyChunk* chunk, Chunks) {
        stationCount += chunk->stationCount();
    }
    return stationCount;
}

/**
  \brief Returns true if this trip has a station with stationName, else returns fales
  */
bool cwTrip::hasStation(QString stationName) const {
    foreach(cwSurveyChunk* chunk, Chunks) {
        if(chunk->hasStation(stationName)) {
            return true;
        }
    }
    return false;
}

/**
  \brief Returns a list of neighboring stations with of stationName, that only exist in this
  trip.

  The trip has a surveyChunks, a survey chunk has shots and stations.  If the stationName exists
  in the trip it will have neighboring stations, if there's at least one shot.
  */
QSet<cwStation> cwTrip::neighboringStations(QString stationName) const {
    QSet<cwStation> neighbors;
    foreach(cwSurveyChunk* chunk, Chunks) {
        QSet<cwStation> chunkNeighbors = chunk->neighboringStations(stationName);
        neighbors.unite(chunkNeighbors);
    }
    return neighbors;
}

/**
  \brief Gets all the unique stations for a trip

  A station may come up more than once on a trip, but only returns
  unique stations for this trip.
  */
QList< cwStation > cwTrip::uniqueStations() const {
    QMap<QString, cwStation> lookup;
    foreach(cwSurveyChunk* chunk, Chunks) {
        for(int i = 0; i < chunk->stationCount(); i++) {
            cwStation station = chunk->station(i);
            lookup[station.name()] = station;
        }
    }
    return lookup.values();
}

/**
  \brief Sets the undo stack for the child objects
  */
void cwTrip::setUndoStackForChildren() {

}

/**
  \brief Set's the parent's cave
  */
void cwTrip::setParentCave(cwCave* parentCave) {
    if(ParentCave != parentCave) {
        ParentCave = parentCave;
        setParent(parentCave);

//        foreach(cwSurveyChunk* chunk, Chunks) {
//            chunk->updateStationsWithNewCave();
//        }

        Notes->setParentCave(ParentCave);
    }
}

cwTrip::NameCommand::NameCommand(cwTrip* trip, QString name) {
    Trip = trip;
    NewName = name;
    OldName = Trip.data()->name();
    setText(QString("Change trip's name to %1").arg(name));
}

void cwTrip::NameCommand::redo() {
    if(Trip.isNull()) { return; }
    Trip.data()->Name = NewName;
    emit Trip.data()->nameChanged(Trip.data()->Name);
}

void cwTrip::NameCommand::undo() {
    if(Trip.isNull()) { return; }
    Trip.data()->Name = OldName;
    emit Trip.data()->nameChanged(Trip.data()->Name);
}

cwTrip::DateCommand::DateCommand(cwTrip* trip, QDate date) {
    Trip = trip;
    NewDate = date;
    OldDate = Trip.data()->date();
    setText(QString("Change trip's date to %1").arg(NewDate.toString(Qt::TextDate)));
}

void cwTrip::DateCommand::redo() {
    if(Trip.isNull()) { return; }
    Trip.data()->Date = NewDate;
    emit Trip.data()->dateChanged(Trip.data()->Date);
}

void cwTrip::DateCommand::undo() {
    if(Trip.isNull()) { return; }
    Trip.data()->Date = OldDate;
    emit Trip.data()->dateChanged(Trip.data()->Date);
}
