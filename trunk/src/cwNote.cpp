#include "cwNote.h"

cwNote::cwNote(QObject *parent) :
    QObject(parent)
{
}

void cwNote::setImagePath(const QString& imagePath) {
    if(ImagePath != imagePath) {
        ImagePath = imagePath;
        emit imagePathChanged();
    }
}




