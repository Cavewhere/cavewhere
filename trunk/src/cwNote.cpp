#include "cwNote.h"

cwNote::cwNote(QObject *parent) :
    QObject(parent)
{
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
    }
}


