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
#include <QDir>
class QOpenGLContext;
class QSurface;

class CAVEWHERE_LIB_EXPORT cwTextureUploadTask
{

public:
    enum Format {
        OpenGL_RGBA,
        Unknown,
    };

    class UploadResult {
    public:
        QImage image; //For RGB data only
        QList< QPair< QByteArray, QSize > > mipmaps; //This isn't use
        QVector2D scaleTexCoords; // This isn't used for RGB
        Format type = Unknown; //This should just be RGB

        bool isNull() const { return image.isNull(); }
    };

    explicit cwTextureUploadTask();

    //Inputs
    void setImage(cwImage image);
    void setDataRootDir(const QDir &dataRootDir);
    void setType(Format type);

    QFuture<cwTextureUploadTask::UploadResult> mipmaps() const;

    static bool isDivisibleBy4(QSize size);
    static Format format();

private:
    Format type = Unknown;
    cwImage Image;
    QDir DataRootDir;


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




inline void cwTextureUploadTask::setType(cwTextureUploadTask::Format type)
{
    this->type = type;
}






#endif // CWTEXTUREUPLOADTASK_H
