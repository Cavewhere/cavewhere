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
#include "cwGlobals.h"
#include "cwDXT1Compresser.h"

//Qt includes
#include <QStringList>
#include <QString>
#include <QImage>
#include <QDir>
#include <QSqlDatabase>
#include <QAtomicInt>
#include <QOpenGLContext>
#include <QOffscreenSurface>

class CompressImageKernal;

class CAVEWHERE_LIB_EXPORT cwAddImageTask : public cwProjectIOTask
{
    friend class CompressImageKernal;

    Q_OBJECT

public:
    cwAddImageTask(QObject* parent = nullptr);
    ~cwAddImageTask();

    //////////////// Parameters //////////////////
    //Option 1 for using
    void setNewImagesPath(QStringList imagePaths);

    //Option 2 for using
    void setNewImages(QList<QImage> images);

    //Option 3 - Regenerate mipmaps
    void setRegenerateMipmapsOn(cwImage image);

    //Process the images
    QFuture<cwImage> images() const;

    //Until functions
    static int numberOfMipmapLevels(QSize imageSize);
    static QSize half(QSize size);
    static QRect half(QRect size);
    static QPoint half(QPoint point);

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

    //Possible image sources
    QStringList NewImagePaths;
    QList<QImage> NewImages;
    cwImage RegenerateMipmap;

    static QImage copyOriginalImage(QString image,
                                    cwImage* imageIds,
                                    QSqlDatabase database);

    static void copyOriginalImage(const QImage& image,
                                  cwImage* imageIds,
                                  QSqlDatabase database);

    static cwImage addImageToDatabase(const QImage& image,
                                      const QByteArray& format,
                                      const QByteArray& imageData,
                                      QSqlDatabase database);

    void createIcon(QImage originalImage, QString imageFilename, cwImage* imageIds);
    void createMipmaps(QImage originalImage, QString imageFilename, cwImage* imageIds);
    int saveToDXT1Format(const cwDXT1Compresser::CompressedImage& image, int id = -1);
    QByteArray squishCompressImageThreaded(QImage image, int flags, float* metric = 0);
    QByteArray openglDxt1Compression(QImage image);
    static QImage ensureImageDivisibleBy4(QImage originalImage, QSizeF* clipArea);

    int dotsPerMeter(QImage image) const;

    static int half(int value);

    static cwImage originalMetaData(const QImage& image);
};

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
  This halves the size.  The size that's returned will always be valid.
  If the half size is less than 1, then the dimension below 1 is set to 1
  */
inline QSize cwAddImageTask::half(QSize size) {
     //Create the new width and height
    return QSize(half(size.width()), half(size.height()));
}

inline QRect cwAddImageTask::half(QRect rect)
{
    return QRect(QPoint(half(rect.x()),
                        half(rect.y())),
                 half(rect.size()));
}

inline QPoint cwAddImageTask::half(QPoint point)
{
    return QPoint(half(point.x()), half(point.y()));
}

inline int cwAddImageTask::half(int value)
{
    return std::max(1, value / 2);
}


#endif // CWLOADIMAGETASK_H
