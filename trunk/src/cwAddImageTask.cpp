//Our includes
#include "cwAddImageTask.h"
#include "cwProject.h"
#include "cwImageData.h"
#include "cwProjectImageProvider.h"
//#include "cwImageDatabase.h"

//For creating compressed DXT texture maps
#include <squish.h>

//Std includes
#include <math.h>

//Qt includes
#include <QString>
#include <QImage>
#include <QGLWidget>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include <QImageReader>
#include <QImageWriter>
#include <QBuffer>

//Sqlite lite includes
#include <sqlite3.h>

//Zlib includes
#include <zlib.h>

//const QString cwAddImageTask::IconsDirectory = "icons";
//const QString cwAddImageTask::MipmapsDirecotry = "mipmaps";

cwAddImageTask::cwAddImageTask(QObject* parent) : cwTask(parent)
{

}

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

    //Try to add the ImagePaths to the database
    tryAddingmagesToDatabase();

    //Close the database
    CurrentDatabase.close();

    if(isRunning()) {
        emit addedImages(images());
    }

    //Finished
    done();




//    //Make sure the icon directory is created
//    BaseDirectory.mkpath("icons");
//    BaseDirectory.mkpath("mipmaps");

//    for(int i = 0; i < ImagePaths.size() && isRunning(); i++) {
//        QString imagePath = ImagePaths[i];

//        //Make sure the image is good, open it
//        QImage image(imagePath);
//        if(image.isNull()) {
//            //Add to errors
//            Errors.append(QString("%1 isn't a image").arg(imagePath));
//            continue;
//        }

//        //Copy the original image
//        QString orginalImagePath = copyOriginalImage(imagePath);

//        if(isRunning()) { break; }

//        //Basename of the image
//        QFileInfo imageInfo(imagePath);
//        QString baseFilename = imageInfo.fileName();

//        //Create an icon
//        QString iconImagePath = createIcon(image, baseFilename);

//        if(isRunning()) { break; }

//        //Create the mipmaps for opengl
//        QStringList mipmapPaths = createMipmaps(image, baseFilename);

//        if(isRunning()) { break; }
//        emit progressed(i + 1);
//    }

//    //Clean up if not running???
//    done();
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
            //The database is busy, try to get a lock in 500ms
            QTimer::singleShot(500, this, SLOT(tryAddingmagesToDatabase()));
            return;
        } else {
            qDebug() << "Load Images error: " << error;
            return;
        }
    }

    //Go through all the images
    for(int i = 0; i < ImagePaths.size() && isRunning(); i++) {
        QString imagePath = ImagePaths[i];

        //Where the database image ideas are stored
        cwImage imageIds;

        //Copy the original image to the database
        QImage originalImage = copyOriginalImage(imagePath, &imageIds);

        //Create a icon image
        createIcon(originalImage, imagePath, &imageIds);

        //Create mipmaps
        createMipmaps(originalImage, imagePath, &imageIds);

        //Add image ids to the list of images that are returned
        Images.append(imageIds);

        emit progressed(i + 1);
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


/**
  \brief This rescales the image by maxSideInPixel

  This is a kernal to QtConnurrent::mapped runTask

  The image that's being scaled set by the construct of the class
  */
//QImage cwAddImageTask::Scaler::operator()(int maxSideInPixels) {
//    return OriginalImage.scaled(maxSideInPixels, maxSideInPixels, Qt::KeepAspectRatio, Qt::SmoothTransformation);
//}

/**
  \brief This copies the original image to the new place
  */
QImage cwAddImageTask::copyOriginalImage(QString imagePath, cwImage* imageIdContainer) {
//    QFileInfo fileInfo(imagePath);
//    QString imageFilename = fileInfo.fileName();
//    QString newFilename = BaseDirectory.absoluteFilePath(imageFilename);

//    //Make sure the image isn't the same basicially check if the images is the same.
//    if(newFilename == imagePath) {
//        //Images are the same, don't copy anything
//        return newFilename;
//    }

    emit statusMessage(QString("Copying %1").arg(QFileInfo(imagePath).fileName()));

    //Copy original directly into the database
    QFile originalFile;
    originalFile.setFileName(imagePath);
    bool successful = originalFile.open(QFile::ReadOnly);

    if(!successful) {
        qDebug() << "Couldn't load image: " << imagePath;
        return QImage();
    }

    //Read the whole file
    QByteArray originalImageByteData = originalFile.readAll();

    //The the original file's format
    QByteArray format = QImageReader::imageFormat(imagePath);

    if(format.isEmpty()) {
        qDebug() << "This file is not an image:" << imagePath;
        return QImage();
    }

    //Load the image
    QImage image;
    image.loadFromData(originalImageByteData, format.constData());

    //Write the image to the database
    cwImageData originalImageData(image.size(), format, originalImageByteData);
    int imageId = cwProject::addImage(CurrentDatabase, originalImageData);
    imageIdContainer->setOriginal(imageId);

    return image;


//    //Get a uniqueFile name for the image
//    QString uniqueFilename = cwProject::uniqueFile(BaseDirectory, imageFilename);

//    //Copy the data
//    QString fullPathToUniqueFilename = BaseDirectory.absoluteFilePath(uniqueFilename);
//    QFile::copy(imagePath, fullPathToUniqueFilename);

//    return fullPathToUniqueFilename;
}

/**
  \brief Creates an icon of the original image

  If the originalImage is less than 512x512, this just save the full image as a icon
  */
void cwAddImageTask::createIcon(QImage originalImage, QString imageFilename, cwImage* imageIds) {
    emit statusMessage(QString("Generating icon for %1").arg(QFileInfo(imageFilename).fileName()));

    QSize scaledSize = QSize(512, 512);
    QImage scaledImage = originalImage.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    //Convert the image into a jpg
    QByteArray format = "jpg";
    QByteArray jpgData;
    QBuffer buffer(&jpgData);
    QImageWriter writer(&buffer, format);
    writer.setCompression(85);
    writer.write(scaledImage);

    //Write the data to database
    cwImageData iconImageData(scaledSize, format, jpgData);
    int imageId = cwProject::addImage(CurrentDatabase, iconImageData);
    imageIds->setIcon(imageId);
}

/**
  \brief This creates compressed mipmaps for the originalImage

  The return stringList is a list of all the mipmaps, starting with level 0 going to level
  size-1 of the list.
  */
void cwAddImageTask::createMipmaps(QImage originalImage, QString imageFilename, cwImage* imageIds) {
    QSize imageSize = originalImage.size();
    double largestDimension = (double)qMax(imageSize.width(), imageSize.height());
    int numberOfLevels = (int)log2(largestDimension);

    QImage scaledImage = originalImage;
    QList<int> mipmapIds;

    int width = scaledImage.width();
    int height = scaledImage.height();

    for(int i = 0; i < numberOfLevels && isRunning(); i++) {
        emit statusMessage(QString("Compressing %1 of %2 the bold flavors of %3").arg(i + 1).arg(numberOfLevels).arg(imageFilename));

        //Rescaled the image
        scaledImage = scaledImage.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        //Export the image to DXT1 format
        int mipmapId = saveToDXT1Format(scaledImage);

        //Add the path to the mipmapPath
        mipmapIds.append(mipmapId);

        //Create the new width and height
        width = qMax(scaledImage.width() / 2, 1);
        height = qMax(scaledImage.height() / 2, 1);
    }

    imageIds->setMipmaps(mipmapIds);
}

/**
  \brief Saves the image to the path using the dxt1 format from squish

  The image will be firsted compress using dxt1 compression 1:6.  Then it'll
  be compressed with zlib to gunzip.  This will compress the image by another
  35% by default.

  \param image - The image that'll be converted
  */
int cwAddImageTask::saveToDXT1Format(QImage image) {
    int dxt1FileSize = squish::GetStorageRequirements(image.width(), image.height(), squish::kDxt1);

    //Allocate the compress data
    QByteArray dxt1FileData;
    dxt1FileData.resize(dxt1FileSize);

    //Convert and compress using dxt1
    QImage convertedFormat = QGLWidget::convertToGLFormat(image);
    squish::CompressImage(static_cast<const squish::u8*>(convertedFormat.bits()),
                          image.width(), image.height(),
                          dxt1FileData.data(),
                          squish::kDxt1 | squish::kColourIterativeClusterFit);

    //Compress the dxt1FileData using zlib
    dxt1FileData = qCompress(dxt1FileData, 9);

    //Add the image to the database
    cwImageData iconImageData(image.size(), cwProjectImageProvider::Dxt1_GZ_Extension, dxt1FileData);
    int imageId = cwProject::addImage(CurrentDatabase, iconImageData);
    return imageId;


//    //Compress the dxt1FileData using zlib
//    path += ".gz";
//    gzFile zippedFile = gzopen(path.toAscii(), "w"); //Write with level 9 huffman
//    gzwrite(zippedFile, dxt1FileData.constData(), dxt1FileData.size());
//    gzclose(zippedFile);

//    //Write data to disk
//    QFile file;
//    file.setFileName(path);
//    file.open(QFile::WriteOnly);

//    if(file.error() != QFile::NoError) {
//        Errors.append(file.errorString());
//        return;
//    }

//    //Write the data to disk
//    file.write(dxt1FileData);

//    if(file.error() != QFile::NoError) {
//        Errors.append(file.errorString());
//        return;
//    }

//    qDebug() << "Wrote compress data to " << file.fileName() << "before zlib compression:" << (dxt1FileSize / 1024.0 / 1024.0) << "after" << (dxt1FileData.size() / 1024.0 / 1024.0);
 }



