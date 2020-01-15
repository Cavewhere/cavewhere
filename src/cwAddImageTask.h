/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLOADIMAGETASK_H
#define CWLOADIMAGETASK_H

//Our includes
#include "cwProjectIOTask.h"
#include "cwImage.h"

//Qt includes
#include <QStringList>
#include <QString>
#include <QImage>
#include <QDir>
#include <QSqlDatabase>
#include <QAtomicInt>
#include <QDebug>
#include <QOpenGLContext>
#include <QOffscreenSurface>

//Squish includes
#include <squish.h>

class CompressImageKernal;

class cwAddImageTask : public cwProjectIOTask
{
    friend class CompressImageKernal;

    Q_OBJECT

public:
    cwAddImageTask(QObject* parent = nullptr);
    ~cwAddImageTask();

    //////////////// Parameters //////////////////
//    void setProjectPath(QString projectPath);

    //Option 1 for using
    void setNewImagesPath(QStringList imagePaths);

    //Option 2 for using
    void setNewImages(QList<QImage> images);

    //Options for adding
    void setMipmapsOnly(bool mipmapOnly);

    //Regenerate mipmaps
    void regenerateMipmapsOn(cwImage image);

    ///////////// Results ///////////////////
    QList<cwImage> images();

signals:
    void addedImages(QList<cwImage> images);

protected:
    virtual void runTask();

private:
    class PrivateImageData {
    public:
        PrivateImageData() { }
        PrivateImageData(cwImage id, QImage original, QString name = QString("default image")) :
            Id(id),
            OriginalImage(original),
            Name(name)
        {

        }


        cwImage Id;
        QImage OriginalImage;
        QString Name;
    };

    QStringList NewImagePaths;
    QList<QImage> NewImages;

    QList<cwImage> Images;
    QStringList Errors;

    bool MipmapOnly; //Doesn't save the original or create an icon

    cwImage RegenerateImage; //This updates the mipmaps for the image

    QAtomicInt Progress;
    QOpenGLContext* CompressionContext;
    QOffscreenSurface* Surface;
    GLuint Texture;

    QImage copyOriginalImage(QString image, cwImage* imageIds);
    void copyOriginalImage(const QImage& image, cwImage* imageIds);
    cwImage addImageToDatabase(const QImage& image, const QByteArray& format, const QByteArray& imageData);

    void createIcon(QImage originalImage, QString imageFilename, cwImage* imageIds);
    void createMipmaps(QImage originalImage, QString imageFilename, cwImage* imageIds);
    int saveToDXT1Format(QImage image, int id = -1);
    QByteArray squishCompressImageThreaded(QImage image, int flags, float* metric = 0);
    QByteArray openglDxt1Compression(QImage image);
    QImage ensureImageDivisibleBy4(QImage originalImage, QSizeF* clipArea);

    void calculateNumberOfSteps();
    int numberOfMipmapLevels(QSize imageSize) const;
    QSize halfSize(QSize size) const;
    int dotsPerMeter(QImage image) const;

    void regenerateMipmaps();

    void IncreaseProgress();

private slots:
    void tryAddingImagesToDatabase();

};

/**
  \brief Sets the project directory for the images

  The images will store the relitive path to the project directory
  */
//inline void cwAddImageTask::setProjectPath(QString projectPath) {
//    DatabasePath = projectPath;
//}

/**
  \brief Sets the databasePath

  This should be set before calling start!  And shouldn't be changed until the task
  has finished
  */
inline void cwAddImageTask::setNewImagesPath(QStringList imagePaths) {
    NewImagePaths = imagePaths;
}

/**
  \brief Sets the images that will be added to the database
  */
inline void cwAddImageTask::setNewImages(QList<QImage> images) {
    NewImages = images;
}

/**
 * @brief cwAddImageTask::setMipmapsOnly
 * @param mipmapOnly - Will only generate the mipmaps of the image
 *
 * This will not save the original image's data, and it will not resize
 * the icon data either. Or save the icon.
 */
inline void cwAddImageTask::setMipmapsOnly(bool mipmapOnly)
{
    MipmapOnly = mipmapOnly;
}

/**
 * @brief cwAddImageTask::regenerateMipmapsOn
 * @param image
 */
inline void cwAddImageTask::regenerateMipmapsOn(cwImage image)
{
   RegenerateImage = image;
}

/**
  Get's all the images that have been put into the database

  \brief This should only be called when the task is not running
  */
inline QList<cwImage> cwAddImageTask::images() {
    return Images;
}

/**
  This halves the size.  The size that's returned will always be valid.
  If the half size is less than 1, then the dimension below 1 is set to 1
  */
inline QSize cwAddImageTask::halfSize(QSize size) const {
     //Create the new width and height
    return QSize(qMax(size.width() / 2, 1), qMax(size.height() / 2, 1));
}


#endif // CWLOADIMAGETASK_H
