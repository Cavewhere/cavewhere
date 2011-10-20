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
    }
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
//QMatrix4x4 cwNote::noteTransformationMatrix() const {
//    //Transformation matrix for maniplating the points
//    cwNoteTranformation transformation = stationNoteTransformation();
//    QMatrix4x4 matrix;
//    matrix.rotate(transformation.northUp(), 0.0, 0.0, 1.0);
//    matrix.scale(transformation.scale(), transformation.scale(), 1.0);
//    matrix = matrix * scaleMatrix().inverted();  //The matrix that converts a real point into a notePosition
//    return matrix;
//}

/**
  \Brief adds a scrap to the notes
  */
void cwNote::addScrap(cwScrap* scrap) {
    if(scrap == NULL) {
        qDebug() << "This is a bug! scrap is null and shouldn't be" << LOCATION;
        return;
    }

    scrap->setParent(this);
    scrap->setParentNote(this);
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
