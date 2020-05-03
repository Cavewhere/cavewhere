/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEXTUREUPLOADTASK_H
#define CWTEXTUREUPLOADTASK_H

//Our includes
#include "cwImage.h"
#include "cwGlobals.h"

//Qt includes
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QVector2D>
#include <QFuture>
class QOpenGLContext;
class QSurface;

class CAVEWHERE_LIB_EXPORT cwTextureUploadTask
{

public:
    enum Format {
        DXT1Mipmaps,
        OpenGL_RGBA,
        Unknown,
    };

    enum Threading {
        NoThreading,
        Threaded
    };

    class UploadResult {
    public:
        QList< QPair< QByteArray, QSize > > mipmaps;
        QVector2D scaleTexCoords;
        Format type = Unknown;
    };

    explicit cwTextureUploadTask();

    //Inputs
    void setImage(cwImage image);
    void setProjectFilename(QString filename);
    void setMipmapLevel(int level);
    void setType(Format type);
    void setThreading(Threading threading);

    QFuture<cwTextureUploadTask::UploadResult> mipmaps() const;

    static bool isDivisibleBy4(QSize size);
    static Format format();

private:
    Format type = Unknown;
    cwImage Image;
    QString ProjectFilename;
    int MipmapLevel = -1;
    Threading CurrentThreading = Threaded;


    void loadMipmapsFromDisk();

    void updateScaleTexCoords();
    
};

/**
 * @brief cwTextureUploadTask::setImage
 * @param image
 */
inline void cwTextureUploadTask::setImage(cwImage image) {
    Image = image;
}

/**
 * @brief cwTextureUploadTask::setProjectFilename
 * @param filename
 */
inline void cwTextureUploadTask::setProjectFilename(QString filename)
{
    ProjectFilename = filename;
}

/**
 * Sets the mipmap level for the task. If unset this will return all the mipmaps for the image.
 * If set, it will only return the requested mipmap level
 */
inline void cwTextureUploadTask::setMipmapLevel(int level)
{
    MipmapLevel = level;
}


inline void cwTextureUploadTask::setType(cwTextureUploadTask::Format type)
{
    this->type = type;
}

/**
 * Texture upload uses Qt concurrent to fetch the upload data or not
 */
inline void cwTextureUploadTask::setThreading(cwTextureUploadTask::Threading threading)
{
    CurrentThreading = threading;
}






#endif // CWTEXTUREUPLOADTASK_H
