/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEXTUREUPLOADTASK_H
#define CWTEXTUREUPLOADTASK_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"
#include "cwGlobals.h"

//Qt includes
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QVector2D>
class QOpenGLContext;
class QSurface;

class CAVEWHERE_LIB_EXPORT cwTextureUploadTask
{
public:
    enum Type {
        DXT1Mipmaps,
        OpenGL_RGBA
    };

    class UploadResult {
    public:
        QList< QPair< QByteArray, QSize > > mipmaps;
        QVector2D scaleTexCoords;
        Type type;
    };

    explicit cwTextureUploadTask();

    //Inputs
    void setImage(cwImage image);
    void setProjectFilename(QString filename);
    void setType(Type type);

    UploadResult operator()() const;

    static bool isDivisibleBy4(QSize size);

private:
    Type type = OpenGL_RGBA;
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


inline void cwTextureUploadTask::setType(cwTextureUploadTask::Type type)
{
    this->type = type;
}






#endif // CWTEXTUREUPLOADTASK_H
