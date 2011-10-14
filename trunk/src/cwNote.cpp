//Our include
#include "cwNote.h"
#include "cwTrip.h"
#include "cwScrap.h"
#include "cwDebug.h"

//Std includes
#include <math.h>

//Qt includes
#include <QDebug>

cwNote::cwNote(QObject *parent) :
    QObject(parent),
    ParentTrip(NULL)
{
    DisplayRotation = 0.0;
}

//void cwNote::setImagePath(const QString& imagePath) {
//    if(ImagePath != imagePath) {
//        ImagePath = imagePath;
//        emit imagePathChanged();
//    }
//}

cwNote::cwNote(const cwNote& object) :
    QObject(NULL),
    ParentTrip(NULL)
{
    copy(object);
}

cwNote& cwNote::operator=(const cwNote& object) {
    if(&object == this) { return *this; }
    copy(object);
    return *this;
}

/**
  Does a deap copy of the note
  */
void cwNote::copy(const cwNote& object) {
    ImageIds = object.ImageIds;
    DisplayRotation = object.DisplayRotation;
}

/**
  \brief Sets the image data for the page of notes

  \param cwImage - This is the object that stores the ids to all the image
  data on disk
  */
void cwNote::setImage(cwImage image) {
    if(ImageIds != image) {
        ImageIds = image;
        emit originalChanged(ImageIds.original());
        emit iconChanged(ImageIds.icon());
        emit imageChanged(ImageIds);
    }
}

/**
  \brief Sets the rotation for the image

  \param degrees - The rotation in degrees
  */
void cwNote::setRotate(float degrees) {
    degrees = fmod((double)degrees, 360.0);
    if(degrees != DisplayRotation) {
        DisplayRotation = degrees;
        emit rotateChanged(DisplayRotation);
    }
}

/**
  \brief Sets the parent trip for this note
  */
void cwNote::setParentTrip(cwTrip* trip) {
    if(ParentTrip != trip) {
        ParentTrip = trip;
        setParent(trip);

        //Go through all the stations and update the cave
        cwCave* cave = NULL;
        if(parentTrip() != NULL) {
            cave = parentTrip()->parentCave();
        }

        for(int i = 0; i < Stations.size(); i++) {
            Stations[i].setCave(cave);
        }

    }
}

/**
  \brief Adds a station to the note
  */
void cwNote::addStation(cwNoteStation station) {

    //Reference the cave to the station
    if(parentTrip() != NULL) {
        station.setCave(parentTrip()->parentCave());
    }

    qDebug() << "Station name:" << station.name();

    Stations.append(station);
    updateNoteTransformation();

    emit stationAdded();
}

/**
  \brief Removes a station from the note

  The index needs to be valid
*/
void cwNote::removeStation(int index) {
    if(index < 0 || index >= Stations.size()) {
        return;
    }

    Stations.removeAt(index);
    updateNoteTransformation();

    emit stationRemoved(index);
}

/**
  \brief Gets the station data at role at noteStationIndex with the role
  */
QVariant cwNote::stationData(StationDataRole role, int noteStationIndex) const {
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
void cwNote::setStationData(StationDataRole role, int noteStationIndex, QVariant value)  {

    //Make sure the noteStationIndex is valid
    if(noteStationIndex < 0 || noteStationIndex >= Stations.size()) {
        return;
    }

    cwNoteStation& noteStation = Stations[noteStationIndex];

    switch(role) {
    case StationName:
        if(noteStation.station().name() != value.toString()) {
            noteStation.setName(value.toString());
            updateNoteTransformation();
            emit stationNameChanged(noteStationIndex);
        }
        break;
    case StationPosition:
        if(noteStation.positionOnNote() != value.toPointF()) {
            noteStation.setPositionOnNote(value.toPointF());
            updateNoteTransformation();
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
cwNoteStation cwNote::station(int stationId) {
    if(stationId < 0 || stationId >= Stations.size()) { return cwNoteStation(); }
    return Stations[stationId];
}

/**
 This goes through all the station in this note and finds the note page's average transformatio.

 This find the average scale and rotatation to north, based on the station locations on the page.
  */
void cwNote::updateNoteTransformation() {
    //Get all the stations that make shots on the page of notes
    QList< QPair<cwNoteStation, cwNoteStation> > shotStations = noteShots();
    QList<cwNoteTranformation> transformations = calculateShotTransformations(shotStations);
    cwNoteTranformation averageTransformation = averageTransformations(transformations);
    NoteTransformation = averageTransformation;

    qDebug() << "NoteTransform:" << NoteTransformation.scale() << NoteTransformation.northUp();
}

/**
  \brief Returns all the shots that are located on the page of notes

  Currently this only works for plan shots.

  This is a helper to updateNoteTransformation
  */
QList< QPair <cwNoteStation, cwNoteStation> > cwNote::noteShots() const {
    if(Stations.size() <= 1) { return QList< QPair<cwNoteStation, cwNoteStation> >(); } //Need more than 1 station to update.

    //Find the valid stations
    QSet<cwNoteStation> validStationsSet;
    foreach(cwNoteStation noteStation, Stations) {
        if(noteStation.station().isValid() && noteStation.station().cave() != NULL) {
            validStationsSet.insert(noteStation);
        }
    }

    //Get the parent trip of for these notes
    cwTrip* trip = parentTrip();

    //Go through all the valid stations get the
    QList<cwNoteStation> validStationList = validStationsSet.toList();

    //Generate all the neighbor list for each station
    QList< QSet< cwStationReference > > stationNeighbors;
    foreach(cwNoteStation station, validStationList) {
        QSet<cwStationReference> neighbors = trip->neighboringStations(station.station().name());
        stationNeighbors.append(neighbors);
    }

    QList< QPair<cwNoteStation, cwNoteStation> > shotList;
    for(int i = 0; i < validStationList.size(); i++) {
        for(int j = i; j < validStationList.size(); j++) {
            cwNoteStation station1 = validStationList[i];
            cwNoteStation station2 = validStationList[j];

            //Get neigbor lookup
            QSet< cwStationReference > neighborsStation1 = stationNeighbors[i];
            QSet< cwStationReference > neighborsStation2 = stationNeighbors[j];

            //See if they make up a shot
            if(neighborsStation1.contains(station2.station()) && neighborsStation2.contains(station1.station())) {
                shotList.append(QPair<cwNoteStation, cwNoteStation>(station1, station2));
            }
        }
    }

    return shotList;
}

/**
  This will create cwNoteTransformation for each shot in the list
  */
QList< cwNoteTranformation > cwNote::calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots) const {
    QList<cwNoteTranformation> transformations;
    for(int i = 0; i < shots.size(); i++) {
        QPair< cwNoteStation, cwNoteStation >& shot = shots[i];
        cwNoteTranformation transformation = calculateShotTransformation(shot.first, shot.second);
        transformations.append(transformation);
    }

    return transformations;
}

/**
  This will caluclate the transfromation between station1 and station2
  */
cwNoteTranformation cwNote::calculateShotTransformation(cwNoteStation station1, cwNoteStation station2) const {
    QVector3D station1RealPos = station1.station().position();
    QVector3D station2RealPos = station2.station().position();

    //Remove the z
    station1RealPos.setZ(0.0);
    station2RealPos.setZ(0.0);

    QVector3D station1NotePos(station1.positionOnNote());
    QVector3D station2NotePos(station2.positionOnNote());

    //Scale the normalized points into pixels
    QMatrix4x4 matrix = scaleMatrix();
    station1NotePos = matrix * station1NotePos;
    station2NotePos = matrix * station2NotePos;

    QVector3D realVector = station2RealPos - station1RealPos;
    QVector3D noteVector = station2NotePos - station1NotePos;

    double noteVectorLength = noteVector.length();
    double realVectorLength = realVector.length();

    //calculate the scale
    double scale = noteVectorLength / realVectorLength;

    //calculate the north
    double angle = acos(QVector3D::dotProduct(realVector.normalized(), noteVector.normalized()));
    angle = angle * 180.0 / acos(-1);

    cwNoteTranformation noteTransform;
    noteTransform.setScale(scale);
    noteTransform.setNorthUp(angle);

    return noteTransform;
}

/**
  This will average all the transformatons into one transfromation
  */
cwNoteTranformation cwNote::averageTransformations(QList< cwNoteTranformation > shotTransforms) {

    if(shotTransforms.empty()) {
        return cwNoteTranformation();
    }

    double angleAverage = 0.0;
    double scaleAverage = 0.0;

    foreach(cwNoteTranformation transformation, shotTransforms) {
        angleAverage  += transformation.northUp();
        scaleAverage += transformation.scale();
    }

    angleAverage = angleAverage / (double)shotTransforms.size();
    scaleAverage = scaleAverage / (double)shotTransforms.size();

    cwNoteTranformation transformation;
    transformation.setNorthUp(angleAverage);
    transformation.setScale(scaleAverage);

    return transformation;
}

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
QString cwNote::guessNeighborStationName(const cwNoteStation& previousStation, QPointF stationNotePosition) {

    if(parentTrip() == NULL) { return QString(); }

    QSet<cwStationReference> neigborStations = parentTrip()->neighboringStations(previousStation.name());

    //Make sure we have neigbors
    if(neigborStations.isEmpty()) {
        return QString();
    }

//    In normalized note coordinates
//    QPointF notePositionVector = stationNotePosition - previousStation.positionOnNote();

    QVector3D previousStationPosition = previousStation.station().position();
    previousStationPosition.setZ(0); //Don't need the z

    QMatrix4x4 noteMatrix = noteTransformationMatrix();

    foreach(cwStationReference station, neigborStations) {
        QVector3D stationPosition = station.position();
        stationPosition.setZ(0);

        //Figure out the predicited position of the station on the notes
        QVector3D vectorBetween = stationPosition - previousStationPosition;
        QVector3D noteVectorBetween = noteMatrix * vectorBetween;
        QVector3D predictedPosition = QVector3D(previousStation.positionOnNote()) + noteVectorBetween;

        //Figure out the error between stationNotePosition and predictedPosition
        QVector3D errorVector = predictedPosition - QVector3D(stationNotePosition);
        double normalizedError = errorVector.length() / noteVectorBetween.length();

        qDebug() << "Normalized error: " << normalizedError;

        if(normalizedError < 0.15) {
            //Station is probably the one
            return QString(station.name());
        }
    }

    //Couldn't find a station
    return QString();
}

/**
  \brief Gets the scaling matrix for the orignial size of the notes
  */
QMatrix4x4 cwNote::scaleMatrix() const {
    QSize size = ImageIds.origianlSize();
    QMatrix4x4 matrix;
    matrix.scale(size.width(), size.height(), 1.0);
    return matrix;
}

/**
  This returns a matrix that's used to convert note coordinates into world coordinates.  Of corse with
  world coordinates, you must first offset the matrix.
  */
QMatrix4x4 cwNote::noteTransformationMatrix() const {
    //Transformation matrix for maniplating the points
    cwNoteTranformation transformation = stationNoteTransformation();
    QMatrix4x4 matrix;
    matrix.rotate(transformation.northUp(), 0.0, 0.0, 1.0);
    matrix.scale(transformation.scale(), transformation.scale(), 1.0);
    matrix = matrix * scaleMatrix().inverted();  //The matrix that converts a real point into a notePosition
    return matrix;
}

/**
  \Brief adds a scrap to the notes
  */
void cwNote::addScrap(cwScrap* scrap) {
    if(scrap == NULL) {
        qDebug() << "This is a bug! scrap is null and shouldn't be" << LOCATION;
        return;
    }

    scrap->setParent(this);
    Scraps.append(scrap);
    emit scrapAdded();
}

/**
  \brief Gets all the scraps from the notes
  */
QList<cwScrap*> cwNote::scraps() {
    return Scraps;
}

/**
  Gets the scrap form the note at a index

  If the scrapIndex is out of bounds, this returns NULL
  */
cwScrap* cwNote::scrap(int scrapIndex) {
    if(scrapIndex >= 0 && scrapIndex < Scraps.size()) {
        return Scraps[scrapIndex];
    }
    return NULL;
}
