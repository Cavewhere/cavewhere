//Our include
#include "cwNote.h"
#include "cwTrip.h"

//Std includes
#include <math.h>

//Qt includes
#include <QDebug>

cwNote::cwNote(QObject *parent) :
    QObject(parent),
    ParentTrip(NULL)
{
    Rotation = 0.0;
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
    Rotation = object.Rotation;
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
    if(degrees != Rotation) {
        Rotation = degrees;
        emit rotateChanged(Rotation);
    }
}

/**
  \brief Sets the parent trip for this note
  */
void cwNote::setParentTrip(cwTrip* trip) {
    if(ParentTrip != trip) {
        ParentTrip = trip;
        setParent(trip);
    }
}

/**
  \brief Adds a station to the note
  */
void cwNote::addStation(cwNoteStation station) {
    Stations.append(station);
    emit stationAdded();
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
    case StaitonPosition:
        return noteStation.positionOnNote();
    }
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
            noteStation.station().setName(value.toString());
            emit stationNameChanged(noteStationIndex);
        }
        break;
    case StaitonPosition:
        if(noteStation.positionOnNote() != value.toPointF()) {
            noteStation.setPositionOnNote(value.toPointF());
            emit stationPositionChanged(noteStationIndex);
        }
        break;
    }
}
