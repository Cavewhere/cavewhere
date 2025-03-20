/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImageValidator.h"

//Qt includes
#include <QFileInfo>
#include <QImageReader>

cwImageValidator::cwImageValidator(QObject *parent) :
    QObject(parent)
{
}

/**
 * @brief cwImageValidator::validateImages
 * @param imagePath
 * @return A list of valid images.
 *
 * This will go through all the images in imagePaths and test if the image exist
 * and it's of a support format.  If it's not, an error message is added.  Empty
 * images are simply ignored and removed from the list.  Only valid images are
 * returned.
 */
QStringList cwImageValidator::validateImages(QStringList imagePaths)
{
    QStringList validImages;
    validImages.reserve(imagePaths.size());

    QString newErrorMessage;

    foreach(QString path, imagePaths) {
        if(!path.isEmpty()) {
            QFileInfo fileInfo(path);
            QString shortPath = fileInfo.fileName();
            if(!fileInfo.exists()) {
                newErrorMessage += QString("\"%1\" doesn't exist\n").arg(shortPath);
                break;
            }

            if(!fileInfo.isReadable()) {
                newErrorMessage += QString("Can't open \"%1\" file isn't readable (permission denied\n").arg(shortPath);
                break;
            }

            if(!QImageReader(path).canRead()) {
                newErrorMessage += QString("\"%1\" isn't a valid image (Corrupted?!)\n").arg(shortPath);
                break;
            }

            validImages.append(path);
        }
    }

    setErrorMessage(newErrorMessage);
    return validImages;
}

/**
 * @brief cwImageValidator::setErrorMessage
 * @param errorMessage
 */
void cwImageValidator::setErrorMessage(QString errorMessage)
{
    //Update the error message
    if(this->errorMessage() != errorMessage) {
        ErrorMessage = errorMessage;
        errorMessageChanged();
    }
}

/**
 * @brief cwImageValidator::clearErrorMessage
 *
 * This clears the error message
 */
void cwImageValidator::clearErrorMessage()
{
    setErrorMessage(QString());
}


