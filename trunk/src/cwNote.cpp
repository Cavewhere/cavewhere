//Our include
#include "cwNote.h"

//Std includes
#include <math.h>

//Qt includes
#include <QDebug>

cwNote::cwNote(QObject *parent) :
    QObject(parent)
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
    QObject(NULL)
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

