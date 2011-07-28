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


