/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImageCleanupTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwScrap.h"
#include "cwSQLManager.h"

//Qt includes
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QSet>

cwImageCleanupTask::cwImageCleanupTask()
{
}


/**
 * @brief cwImageCleanupTask::runTask
 *
 * Delete's unused images from the database
 */
void cwImageCleanupTask::runTask()
{
    //Connect to the database
    bool connected = connectToDatabase("UnusedImagesCleanupTask");

    if(connected) {
        tryFetchAllImageIds();

        QSet<int> validIds = extractAllValidImageIds();
        QSet<int> unusedIds = DatabaseIds.subtract(validIds);

        beginTransation();

        QString SQL("DELETE FROM Images WHERE id == ?");
        QSqlQuery removeImageIdQuery(database());

        bool successful = removeImageIdQuery.prepare(SQL);
        if(!successful) {
            qDebug() << "Couldn't delete images:" << removeImageIdQuery.lastError() << LOCATION;
            stop();
            endTransation();
            return;
        }

        foreach(int id, unusedIds) {
            removeImageIdQuery.bindValue(0, id);
            removeImageIdQuery.exec();
        }

        endTransation();

        //Close the database
        disconnectToDatabase();
    }

    done();
}


/**
 * @brief cwImageCleanupTask::tryFetchAllImageIds
 */
void cwImageCleanupTask::tryFetchAllImageIds()
{
    cwSQLManager::Transaction transaction(database(), cwSQLManager::ReadOnly);

    QString sql("select id from images");
    QSqlQuery imageIdsQuery(sql, database());

    QSet<int> ids;
    while(imageIdsQuery.next()) {
        int id = imageIdsQuery.value(0).toInt();
        ids.insert(id);
    }

    DatabaseIds = ids;

}

/**
 * @brief cwImageCleanupTask::extractAllValidImageIds
 * @return All the valid image ids in the database
 *
 * This will go through all the cavewhere structure and add all id's to the
 * set of valid Ids
 */
QSet<int> cwImageCleanupTask::extractAllValidImageIds()
{
    QSet<int> ids;

    foreach(cwCave* cave, Region->caves()) {
        foreach(cwTrip* trip, cave->trips()) {
            foreach(cwNote* note, trip->notes()->notes()) {
                cwImage image = note->image();
                QSet<int> imageIds = imageToSet(image);
                ids = ids.unite(imageIds);

                foreach(cwScrap* scrap, note->scraps()) {
                    image = scrap->triangulationData().croppedImage();
                    imageIds = imageToSet(image);
                    ids = ids.unite(imageIds);
                }
            }
        }
    }

    return ids;
}

/**
 * @brief cwImageCleanupTask::imageToSet
 * @param image
 * @return The converted image into a set of ids
 */
QSet<int> cwImageCleanupTask::imageToSet(cwImage image) const
{
    QSet<int> ids;
    ids.insert(image.icon());
    ids.insert(image.original());

    foreach(int mipmapId, image.mipmaps()) {
        ids.insert(mipmapId);
    }

    return ids;
}

