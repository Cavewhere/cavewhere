/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our project
#include "cwRemoveImageTask.h"
#include "cwProject.h"

cwRemoveImageTask::cwRemoveImageTask(QObject *parent)
    : cwProjectIOTask(parent)
{

}

/**
 * @brief cwRemoveImageTask::setImagesToRemove
 * @param images - Sets the images to be removed
 */
void cwRemoveImageTask::setImagesToRemove(QList<cwImage> images)
{
    Images = images;
}

void cwRemoveImageTask::runTask()
{
    //Connect to the database
    bool connected = connectToDatabase("RemoveImagesTask");

    if(connected) {
        //Try to add the ImagePaths to the database
        tryToRemoveImages();

        //Close the database
        Database.close();
    }

    done();
}

/**
 * @brief cwRemoveImageTask::tryToRemoveImages
 *
 * Try to remove all the images from the database
 */
void cwRemoveImageTask::tryToRemoveImages()
{
    //Try to start a transaction
    bool good = beginTransation(SLOT(tryToRemoveImages()));
    if(!good) { return; }

    foreach(cwImage image, Images) {
        if(image.isValid()) {
            cwProject::removeImage(Database, image);
        }
    }

    endTransation();
}
