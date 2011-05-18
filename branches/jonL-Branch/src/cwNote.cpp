#include "cwNote.h"

cwNote::cwNote(QObject *parent) :
    QObject(parent)
{
}

void cwNote::setImagePath(const QString& imagePath) {
    if(ImagePath != imagePath) {
        ImagePath = imagePath;
        emit imagePathChanged();
        emit imageChanged();
    }
}

/**
  \brief Get's the image for the page of notes
  */
QImage cwNote::image() {
    if(Image.isNull()) {
        Image = QImage(ImagePath);
    }
    return Image;
}



