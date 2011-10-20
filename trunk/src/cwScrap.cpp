//Our includes
#include "cwScrap.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwNote.h"
#include "cwDebug.h"

//Qt includes
#include <QDebug>

cwScrap::cwScrap(QObject *parent) :
    QObject(parent),
    ParentNote(NULL)
{

}

/**
  \brief Inserts a point in scrap
  */
void cwScrap::insertPoint(int index, QPointF point) {
    if(index < 0 || index > OutlinePoints.size()) {
        qDebug() << "Inserting point out of bounds:" << point << index << LOCATION;
    }

    OutlinePoints.insert(index, point);
    emit insertedPoints(index, index);
}

/**
  \brief Inserts a point in scrap
  */
void cwScrap::removePoint(int index) {
    if(index < 0 || index >= OutlinePoints.size()) {
        qDebug() << "Removing point out of bounds:" << index << LOCATION;
        return;
    }

    OutlinePoints.remove(index);
    emit removedPoints(index, index);
}

/**
  \brief Adds a station to the note
  */
void cwScrap::addStation(cwNoteStation station) {

    //Reference the cave to the station
    if(parentNote() != NULL && parentNote()->parentTrip() != NULL) {
        station.setCave(parentNote()->parentTrip()->parentCave());
    }

    qDebug() << "Station name:" << station.name();

    Stations.append(station);
//    updateNoteTransformation();

    emit stationAdded();
}

/**
  \brief Removes a station from the note

  The index needs to be valid
*/
void cwScrap::removeStation(int index) {
    if(index < 0 || index >= Stations.size()) {
        return;
    }

    Stations.removeAt(index);
//    updateNoteTransformation();

    emit stationRemoved(index);
}

/**
  \brief Gets the station data at role at noteStationIndex with the role
  */
QVariant cwScrap::stationData(StationDataRole role, int noteStationIndex) const {
    //Make sure the noteStationIndex is valid
    if(noteStationIndex < 0 || noteStationIndex >= Stations.size()) {
        return QVariant();
    }

    cwNoteStation noteStation = Stations[noteStationIndex];

    switch(role) {
    case StationName:
        return noteStation.station().name();
    case StationPosition:
        return noteStation.positionOnNote();
    }
    return QVariant();
}

/**
  \brief Sets the station data on the note
  */
void cwScrap::setStationData(StationDataRole role, int noteStationIndex, QVariant value)  {

    //Make sure the noteStationIndex is valid
    if(noteStationIndex < 0 || noteStationIndex >= Stations.size()) {
        return;
    }

    cwNoteStation& noteStation = Stations[noteStationIndex];

    switch(role) {
    case StationName:
        if(noteStation.station().name() != value.toString()) {
            noteStation.setName(value.toString());
//            updateNoteTransformation();
            emit stationNameChanged(noteStationIndex);
        }
        break;
    case StationPosition:
        if(noteStation.positionOnNote() != value.toPointF()) {
            noteStation.setPositionOnNote(value.toPointF());
//            updateNoteTransformation();
            emit stationPositionChanged(noteStationIndex);
        }
        break;
    }
}

/**
  \brief Gets the station at stationId

  stationId - The station at stationId, if the station id is invalid, this returns
  an empty cwNoteStation
  */
cwNoteStation cwScrap::station(int stationId) {
    if(stationId < 0 || stationId >= Stations.size()) { return cwNoteStation(); }
    return Stations[stationId];
}

///**
// This goes through all the station in this note and finds the note page's average transformatio.

// This find the average scale and rotatation to north, based on the station locations on the page.
//  */
//void cwScrap::updateNoteTransformation() {
//    //Get all the stations that make shots on the page of notes
//    QList< QPair<cwNoteStation, cwNoteStation> > shotStations = noteShots();
//    QList<cwNoteTranformation> transformations = calculateShotTransformations(shotStations);
//    cwNoteTranformation averageTransformation = averageTransformations(transformations);
//    NoteTransformation = averageTransformation;

//    qDebug() << "NoteTransform:" << NoteTransformation.scale() << NoteTransformation.northUp();
//}

///**
//  \brief Returns all the shots that are located on the page of notes

//  Currently this only works for plan shots.

//  This is a helper to updateNoteTransformation
//  */
//QList< QPair <cwNoteStation, cwNoteStation> > cwScrap::noteShots() const {
//    if(Stations.size() <= 1) { return QList< QPair<cwNoteStation, cwNoteStation> >(); } //Need more than 1 station to update.
//    if(parentNote() == NULL || parentNote()->parentTrip() == NULL) { return QList< QPair<cwNoteStation, cwNoteStation> >(); }

//    //Find the valid stations
//    QSet<cwNoteStation> validStationsSet;
//    foreach(cwNoteStation noteStation, Stations) {
//        if(noteStation.station().isValid() && noteStation.station().cave() != NULL) {
//            validStationsSet.insert(noteStation);
//        }
//    }

//    //Get the parent trip of for these notes
//    cwTrip* trip = parentNote()->parentTrip();

//    //Go through all the valid stations get the
//    QList<cwNoteStation> validStationList = validStationsSet.toList();

//    //Generate all the neighbor list for each station
//    QList< QSet< cwStationReference > > stationNeighbors;
//    foreach(cwNoteStation station, validStationList) {
//        QSet<cwStationReference> neighbors = trip->neighboringStations(station.station().name());
//        stationNeighbors.append(neighbors);
//    }

//    QList< QPair<cwNoteStation, cwNoteStation> > shotList;
//    for(int i = 0; i < validStationList.size(); i++) {
//        for(int j = i; j < validStationList.size(); j++) {
//            cwNoteStation station1 = validStationList[i];
//            cwNoteStation station2 = validStationList[j];

//            //Get neigbor lookup
//            QSet< cwStationReference > neighborsStation1 = stationNeighbors[i];
//            QSet< cwStationReference > neighborsStation2 = stationNeighbors[j];

//            //See if they make up a shot
//            if(neighborsStation1.contains(station2.station()) && neighborsStation2.contains(station1.station())) {
//                shotList.append(QPair<cwNoteStation, cwNoteStation>(station1, station2));
//            }
//        }
//    }

//    return shotList;
//}

///**
//  This will create cwNoteTransformation for each shot in the list
//  */
//QList< cwNoteTranformation > cwScrap::calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots) const {
//    QList<cwNoteTranformation> transformations;
//    for(int i = 0; i < shots.size(); i++) {
//        QPair< cwNoteStation, cwNoteStation >& shot = shots[i];
//        cwNoteTranformation transformation = calculateShotTransformation(shot.first, shot.second);
//        transformations.append(transformation);
//    }

//    return transformations;
//}

///**
//  This will caluclate the transfromation between station1 and station2
//  */
//cwNoteTranformation cwScrap::calculateShotTransformation(cwNoteStation station1, cwNoteStation station2) const {
//    QVector3D station1RealPos = station1.station().position();
//    QVector3D station2RealPos = station2.station().position();

//    //Remove the z
//    station1RealPos.setZ(0.0);
//    station2RealPos.setZ(0.0);

//    QVector3D station1NotePos(station1.positionOnNote());
//    QVector3D station2NotePos(station2.positionOnNote());

//    //Scale the normalized points into pixels
//    QMatrix4x4 matrix = scaleMatrix();
//    station1NotePos = matrix * station1NotePos;
//    station2NotePos = matrix * station2NotePos;

//    QVector3D realVector = station2RealPos - station1RealPos;
//    QVector3D noteVector = station2NotePos - station1NotePos;

//    double noteVectorLength = noteVector.length();
//    double realVectorLength = realVector.length();

//    //calculate the scale
//    double scale = noteVectorLength / realVectorLength;

//    //calculate the north
//    double angle = acos(QVector3D::dotProduct(realVector.normalized(), noteVector.normalized()));
//    angle = angle * 180.0 / acos(-1);

//    cwNoteTranformation noteTransform;
//    noteTransform.setScale(scale);
//    noteTransform.setNorthUp(angle);

//    return noteTransform;
//}

///**
//  This will average all the transformatons into one transfromation
//  */
//cwNoteTranformation cwScrap::averageTransformations(QList< cwNoteTranformation > shotTransforms) {

//    if(shotTransforms.empty()) {
//        return cwNoteTranformation();
//    }

//    double angleAverage = 0.0;
//    double scaleAverage = 0.0;

//    foreach(cwNoteTranformation transformation, shotTransforms) {
//        angleAverage  += transformation.northUp();
//        scaleAverage += transformation.scale();
//    }

//    angleAverage = angleAverage / (double)shotTransforms.size();
//    scaleAverage = scaleAverage / (double)shotTransforms.size();

//    cwNoteTranformation transformation;
//    transformation.setNorthUp(angleAverage);
//    transformation.setScale(scaleAverage);

//    return transformation;
//}

/**
  This guess the station based on the position on the Notes

  If it doesn't know the station name.  If a station is within 5%, then it'll automatically
  update the station name.

  This only works for the plan view, z is always 0

  \param previousStation - The current station that's select.  This will only look at neighboring
  station of this station
  \param stationNotePosition - The position on this page of notes of the station that needs to be guessed

  \return An empty string if no station is within 5% of the guess
  */
QString cwScrap::guessNeighborStationName(const cwNoteStation& previousStation, QPointF stationNotePosition) {

//    if(parentNote() == NULL) { return QString(); }

//    QSet<cwStationReference> neigborStations = parentNote()->neighboringStations(previousStation.name());

//    //Make sure we have neigbors
//    if(neigborStations.isEmpty()) {
//        return QString();
//    }

////    In normalized note coordinates
////    QPointF notePositionVector = stationNotePosition - previousStation.positionOnNote();

//    QVector3D previousStationPosition = previousStation.station().position();
//    previousStationPosition.setZ(0); //Don't need the z

//    QMatrix4x4 noteMatrix = noteTransformationMatrix();

//    foreach(cwStationReference station, neigborStations) {
//        QVector3D stationPosition = station.position();
//        stationPosition.setZ(0);

//        //Figure out the predicited position of the station on the notes
//        QVector3D vectorBetween = stationPosition - previousStationPosition;
//        QVector3D noteVectorBetween = noteMatrix * vectorBetween;
//        QVector3D predictedPosition = QVector3D(previousStation.positionOnNote()) + noteVectorBetween;

//        //Figure out the error between stationNotePosition and predictedPosition
//        QVector3D errorVector = predictedPosition - QVector3D(stationNotePosition);
//        double normalizedError = errorVector.length() / noteVectorBetween.length();

//        qDebug() << "Normalized error: " << normalizedError;

//        if(normalizedError < 0.15) {
//            //Station is probably the one
//            return QString(station.name());
//        }
//    }

    //Couldn't find a station
    return QString();
}

/**
  \brief Sets the parent trip
  */
void cwScrap::setParentNote(cwNote* note) {
    if(ParentNote != note) {
        if(ParentNote != NULL) {
            disconnect(ParentNote, NULL, this, NULL);
        }

        ParentNote = note;

        if(ParentNote != NULL) {
            connect(ParentNote, SIGNAL(parentTripChanged()), SLOT(updateStationsWithNewCave()));
        }

        updateStationsWithNewCave();
    }
}

void cwScrap::updateStationsWithNewCave() {
    //Go through all the stations and update the cave
    cwCave* cave = NULL;
    if(parentNote() != NULL) {
        if(parentNote()->parentTrip() != NULL) {
            cave = parentNote()->parentTrip()->parentCave();
        }
    }

    for(int i = 0; i < Stations.size(); i++) {
        Stations[i].setCave(cave);
    }
}
