//Our includes
#include "cwAddImageTask.h"
#include "cwProject.h"
//#include "cwImageData.h"
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

const QString cwAddImageTask::IconsDirectory = "icons";
const QString cwAddImageTask::MipmapsDirecotry = "mipmaps";

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

    //Make sure the icon directory is created
    BaseDirectory.mkpath("icons");
    BaseDirectory.mkpath("mipmaps");

    for(int i = 0; i < ImagePaths.size() && status() == cwTask::Running; i++) {
        QString imagePath = ImagePaths[i];

        //Make sure the image is good, open it
        QImage image(imagePath);
        if(image.isNull()) {
            //Add to errors
            Errors.append(QString("%1 isn't a image").arg(imagePath));
            continue;
        }

        //Copy the original image
        copyOriginalImage(imagePath);

        if(status() != cwTask::Running) { break; }

        //Basename of the image
        QFileInfo imageInfo(imagePath);
        QString baseFilename = imageInfo.fileName();

        //Create an icon
        createIcon(image, baseFilename);

        if(status() != cwTask::Running) { break; }

        //Create the mipmaps for opengl
        createMipmaps(image, baseFilename);

        if(status() != cwTask::Running) { break; }
        emit progressed(i + 1);
    }

    //Clean up if not running???
    done();
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
  \brief This copies the original image to the new place
  */
QString cwAddImageTask::copyOriginalImage(QString imagePath) {
    QFileInfo fileInfo(imagePath);
    QString imageFilename = fileInfo.fileName();
    QString newFilename = BaseDirectory.absoluteFilePath(imageFilename);

    //Make sure the image isn't the same basicially check if the images is the same.
    if(newFilename == imagePath) {
        //Images are the same, don't copy anything
        return newFilename;
    }

    emit statusMessage(QString("Copying %1").arg(imageFilename));

    //Get a uniqueFile name for the image
    QString uniqueFilename = cwProject::uniqueFile(BaseDirectory, imageFilename);

    //Copy the data
    QString fullPathToUniqueFilename = BaseDirectory.absoluteFilePath(uniqueFilename);
    QFile::copy(imagePath, fullPathToUniqueFilename);

    return fullPathToUniqueFilename;
}

/**
  \brief Creates an icon of the original image

  If the originalImage is less than 512x512, this just save the full image as a icon
  */
QString cwAddImageTask::createIcon(QImage originalImage, QString baseFilename) {
    emit statusMessage(QString("Generating icon for %1").arg(baseFilename));

    QImage scaledImage = originalImage.scaled(QSize(512, 512), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QString scaledImagePath = BaseDirectory.absoluteFilePath(QString("%1/%2.jpg")
                                                             .arg(IconsDirectory)
                                                             .arg(baseFilename));
    scaledImage.save(scaledImagePath, "jpg", 85); //85 percent quality

    return scaledImagePath;
}

/**
  \brief This creates compressed mipmaps for the originalImage

  The return stringList is a list of all the mipmaps, starting with level 0 going to level
  size-1 of the list.
  */
QStringList cwAddImageTask::createMipmaps(QImage originalImage, QString baseFilename) {

    QSize imageSize = originalImage.size();
    double largestDimension = (double)qMax(imageSize.width(), imageSize.height());
    int numberOfLevels = (int)log2(largestDimension);

    QImage scaledImage = originalImage;
    QStringList mipmapPaths;

    int width = scaledImage.width();
    int height = scaledImage.height();

    for(int i = 0; i < numberOfLevels && status() == cwTask::Running; i++) {
        emit statusMessage(QString("Compressing %1 of %2 the bold flavors of %3").arg(i + 1).arg(numberOfLevels).arg(baseFilename));

        //Rescaled the image
        scaledImage = scaledImage.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        //Export the image to DXT1 format
        QString mipmapImagePath = BaseDirectory.absoluteFilePath("%1/%2-level%3.dxt1")
                .arg(MipmapsDirecotry)
                .arg(baseFilename)
                .arg(i);
        saveToDXT1Format(scaledImage, mipmapImagePath);

        //Add the path to the mipmapPath
        mipmapPaths.append(mipmapImagePath);

        //Create the new width and height
        width = qMax(scaledImage.width() / 2, 1);
        height = qMax(scaledImage.height() / 2, 1);
    }

    return mipmapPaths;
}

/**
  \brief Saves the image to the path using the dxt1 format from squish

  \param image - The image that'll be converted
  \param path - The path when the image will be saved to
  */
void cwAddImageTask::saveToDXT1Format(QImage image, QString path) {
    int dxt1FileSize = squish::GetStorageRequirements(image.width(), image.height(), squish::kDxt1);

    //Allocate the compress data
    QByteArray dxt1FileData;
    dxt1FileData.resize(dxt1FileSize);

    //Convert and compress using dxt1
    QImage convertedFormat = QGLWidget::convertToGLFormat(image);
    squish::CompressImage(static_cast<const squish::u8*>(convertedFormat.bits()),
                          image.width(), image.height(),
                          dxt1FileData.data(),
                          squish::kDxt1);

    //Compress the dxt1FileData using zlib
    dxt1FileData = qCompress(dxt1FileData, 9);

    //Write data to disk
    QFile file;
    file.setFileName(path);
    file.open(QFile::WriteOnly);

    if(file.error() != QFile::NoError) {
        Errors.append(file.errorString());
        return;
    }

    //Write the data to disk
    file.write(dxt1FileData);

    if(file.error() != QFile::NoError) {
        Errors.append(file.errorString());
        return;
    }

    qDebug() << "Wrote compress data to " << file.fileName() << "before zlib compression:" << (dxt1FileSize / 1024.0 / 1024.0) << "after" << (dxt1FileData.size() / 1024.0 / 1024.0);
 }


