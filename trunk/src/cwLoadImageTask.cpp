//Our includes
#include "cwLoadImageTask.h"
#include "cwImageData.h"
#include "cwImageDatabase.h"

//Qt includes
#include <QImage>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QtConcurrentMap>
#include <QBuffer>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>

#define SQLITE_BUSY         5

cwAddImageTask::cwAddImageTask(QObject* parent) : cwTask(parent)
{

}

///**
//  This creates the query that inserts th
//  */
//void cwAddImageTask::addImageToDatabase(QSqlDatabase& database, QSize size, QString type, const QByteArray& image) {

//}

/**
  \brief This loads images from disk into the database

  This will mipmap the images as well, create a icon image.  The original is also stored
  */
void cwAddImageTask::runTask() {

    //Set the number of steps for this task
    setNumberOfSteps(ImagePaths.size());

    //Connect to the database
    CurrentDatabase = QSqlDatabase::addDatabase("QSQLITE", "LoadImageTask");
    CurrentDatabase.setDatabaseName(DatabasePath);
    bool connected = CurrentDatabase.open();
    if(!connected) {
        qDebug() << "Couldn't connect to database for LoadImageTask:" << DatabasePath;
        stop();
    }

    //Try to add thi ImagePaths to the database
    tryAddingmagesToDatabase();

    //Close the database
    CurrentDatabase.close();
}

/**
  \brief This rescales the image by maxSideInPixel

  This is a kernal to QtConnurrent::mapped runTask

  The image that's being scaled set by the construct of the class
  */
QImage cwAddImageTask::Scaler::operator()(int maxSideInPixels) {
    return OriginalImage.scaled(maxSideInPixels, maxSideInPixels, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

/**
  \brief This tries to add the image to the database
  */
void cwAddImageTask::tryAddingmagesToDatabase() {
    //SQLITE begin transation
    QString beginTransationQuery = "BEGIN IMMEDIATE TRANSACTION";
    QSqlQuery query = CurrentDatabase.exec(beginTransationQuery);
    QSqlError error = query.lastError();

    //Check if there's error
    if(error.isValid()) {
        if(error.number() == SQLITE_BUSY) {
            //The database is busy, try to get a lock in 100ms
            QTimer::singleShot(100, this, SLOT(tryAddingmagesToDatabase()));
            return;
        } else {
            qDebug() << "Load Images error: " << error;
            return;
        }
    }

    //Go through all the images
    for(int i = 0; i < ImagePaths.size() && isRunning(); i++) {
        QString imagePath = ImagePaths[i];

        //Copy original directly into the database
        QFile originalFile;
        originalFile.setFileName(imagePath);
        bool successful = originalFile.open(QFile::ReadOnly);

        if(!successful) {
            qDebug() << "Couldn't load image: " << imagePath;
            continue;
        }

        //Read the whole file
        QByteArray originalImageByteData = originalFile.readAll();

        //The the original file's format
        QByteArray format = QImageReader::imageFormat(imagePath);

        if(format.isEmpty()) {
            qDebug() << "This file is not an image:" << imagePath;
            continue;
        }

        //Load the image
        QImage image;
        image.loadFromData(originalImageByteData, format.constData());

        //Write the image to the database
        cwImageData originalImageData(image.size(), format, originalImageByteData);
        int imageId = cwImageDatabaseManager::addImageToDatabase(CurrentDatabase, originalImageData);

        qDebug() << "Last inserted imageId:" << imagePath << imageId;


        //Get the extension



//        //Make the icon
//        QFuture iconFuture<QImage>;
//        if(qMax(image.size().width(), image.size().height()) > 512) {
//           QList<int> iconImageSize;
//           iconImageSize.append(512);
//           iconFuture = QtConcurrent::mapped(iconImageSize, Scale(image));
//        }

//        //Create the rescale sizes for mipmaps
//        QList<int> mipmapImageSize;
//        int imageMinSize = qMin(image.size().height(), image.size().width());

//        //Create the resize list
//        while(imageMinSize > 4) {
//            mipmapImageSize.append(imageMinSize);
//            imageMinSize = imageMinSize / 2;
//        }

//        //Rescale the mipmaps
//        QFuture mipmapFuture<QImage> = QtConcurrent::mapped(mipmapImageSize, Scale(image));

//        //Get the iconFuture if doesn't already exist444
//        if(iconFuture.resultCount() != 0) {
//            QImage iconImage = iconFuture.result();
//            QScopedPointer<QBuffer> buffer = new QBuffer();
//            QString imageType = "jpg";
//            iconImage.save(buffer, imageType, 85);

//            addImageToDatabase(database, iconImage.size(), imageType, buffer->data());
//        }
    }

    //If the task isn't running
    if(isRunning()) {
        //Commit the data
        QString commitTransationQuery = "COMMIT";
        QSqlQuery query = CurrentDatabase.exec(commitTransationQuery);
        if(query.lastError().isValid()) {
            qDebug() << "Couldn't commit transaction:" << query.lastError();
        }
    } else {
        //Roll back the commited images
        QString rollbackTransationQuery = "ROLLBACK";
        QSqlQuery query = CurrentDatabase.exec(rollbackTransationQuery);
        if(query.lastError().isValid()) {
            qDebug() << "Couldn't rollback transaction:" << query.lastError();
        }
    }
}
