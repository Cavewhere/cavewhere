//Our includes
#include "cwImageDatabase.h"
#include "cwProject.h"
#include "cwSQLManager.h"
#include "cwDebug.h"

//Qt includes
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

cwImageDatabase::cwImageDatabase(const QString& filename)
{
    setFilename(filename);
}

cwImageDatabase::~cwImageDatabase()
{
    if(Database.isOpen()) {
        Database.close();
    }
}

void cwImageDatabase::setFilename(const QString& filename)
{

    if(Database.isOpen()) {
        Database.close();
    }

    if(filename.isEmpty()) {
        Database.setDatabaseName(filename);
        return;
    }

    Database = cwProject::createDatabaseConnection("cwImageDatabase", filename);
}

/**
  \brief Adds an image to the project file

  This static function takes a database and adds the imageData to the database

  This returns the id of the image in the database
  */
int cwImageDatabase::addImage(const cwImageData& imageData) {
    cwSQLManager::Transaction transaction(Database);

    QString SQL = "INSERT INTO Images (type, shouldDelete, width, height, dotsPerMeter, imageData) "
            "VALUES (?, ?, ?, ?, ?, ?)";

    QSqlQuery query(Database);
    bool successful = query.prepare(SQL);

    if(!successful) {
        qDebug() << "Couldn't create Insert Images query: " << query.lastError() << LOCATION;
        return -1;
    }

    query.bindValue(0, imageData.format());
    query.bindValue(1, false);
    query.bindValue(2, imageData.size().width());
    query.bindValue(3, imageData.size().height());
    query.bindValue(4, imageData.dotsPerMeter());
    query.bindValue(5, imageData.data());
    query.exec();

    //Get the id of the last inserted id
    return query.lastInsertId().toInt();
}

/**
 * @brief cwImageDatabase::updateImage
 * @param database - The database where the image is going to be inserted into
 * @param imageData - The data that going to update the image
 * @param id - The id of the image that needs to be updated
 * @return True if image was update successfully and false, if unsuccessful
 */
bool cwImageDatabase::updateImage(const cwImageData &imageData, int id)
{
    cwSQLManager::Transaction transaction(Database);

    QString SQL("UPDATE Images SET type=?, width=?, height=?, dotsPerMeter=?, imageData=? where id=?");

    QSqlQuery query(Database);
    bool successful = query.prepare(SQL);

    if(!successful) {
        qDebug() << "Couldn't create Insert Images query: " << query.lastError() << LOCATION;
        return false;
    }

    query.bindValue(0, imageData.format());
    query.bindValue(1, imageData.size().width());
    query.bindValue(2, imageData.size().height());
    query.bindValue(3, imageData.dotsPerMeter());
    query.bindValue(4, imageData.data());
    query.bindValue(5, id);
    return query.exec();
}

int cwImageDatabase::addOrUpdateImage(const cwImageData &imageData, int id)
{
    if(id > 0 && imageExists(id)) {
        bool okay = updateImage(imageData, id);
        if(!okay) {
            return -1;
        }
    } else {
        id = addImage(imageData);
    }
    return id;
}

bool cwImageDatabase::imageExists(int id) const
{
    QString SQL("SELECT count(*) as count from Images where id=?");

    QSqlQuery query(Database);
    bool successful = query.prepare(SQL);

    if(!successful) {
        qDebug() << "Couldn't create imageExists query: " << query.lastError() << LOCATION;
        return false;
    }

    query.bindValue(0, id);
    bool queryOkay = query.exec();

    if(!queryOkay) {
        qDebug() << "imageExists failed to exec: " << query.lastError() << LOCATION;
        return false;
    }

    query.first();
    auto record = query.record();
    int count = record.value(0).toInt();
    return count == 1;
}

bool cwImageDatabase::mipmapsValid(cwImage image, bool usingCompression) const
{
    if(image.isOriginalValid()) {
        //Should be in the database
        if(usingCompression) {
            if(!image.isMipmapsValid()) {
                return false;
            }
            foreach(int mipmap, image.mipmaps()) {
                if(!imageExists(mipmap)) {
                    return false;
                }
            }
            return true;
        }
        return imageExists(image.original());
    }
    return false;
}

/**
 * @brief cwImageDatabase::removeImage
 * @param database - The database connection
 * @param image - The Image that going to be removed
 * @return True if the image could be removed, and false if it couldn't be removed
 */
bool cwImageDatabase::removeImage(cwImage image, bool withTransaction)
{
    return removeImages(image.ids(), withTransaction);
}

bool cwImageDatabase::removeImages(QList<int> ids, bool withTransaction)
{
    if(ids.isEmpty()) {
        return true;
    }

    if(withTransaction) {
        cwSQLManager::instance()->beginTransaction(Database);
    }

    auto deleteImage = [&](int id) {
        //Create the delete SQL statement
        QString SQL = QString("DELETE FROM Images WHERE id == %1").arg(id);

        QSqlQuery query(Database);
        bool successful = query.prepare(SQL);

        if(!successful) {
            qDebug() << "Couldn't delete image: " << id << query.lastError();

            if(withTransaction) {
                cwSQLManager::instance()->endTransaction(Database);
            }

            return false;
        }

        return query.exec();
    };

    bool okay = true;
    for(int id : ids) {
        okay = okay && deleteImage(id);
    }

    if(withTransaction) {
        cwSQLManager::instance()->endTransaction(Database);
    }

    return okay;
}
