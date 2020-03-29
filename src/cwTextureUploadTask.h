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
#include "cwJob.h"

//Qt includes
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QVector2D>
#include <QFuture>
class QOpenGLContext;
class QSurface;

class CAVEWHERE_LIB_EXPORT cwTextureUploadTask : public cwJob
{

public:
    enum Format {
        DXT1Mipmaps,
        OpenGL_RGBA,
        Unknown,
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
    void setType(Format type);

    QFuture<cwTextureUploadTask::UploadResult> mipmaps() const;

    static bool isDivisibleBy4(QSize size);
    static Format format();

private:
    Format type = Unknown;
    cwImage Image;
    QString ProjectFilename;


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


inline void cwTextureUploadTask::setType(cwTextureUploadTask::Format type)
{
    this->type = type;
}






#endif // CWTEXTUREUPLOADTASK_H
