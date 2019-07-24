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

//Qt includes
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QVector2D>
class QOpenGLContext;
class QSurface;

class cwTextureUploadTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwTextureUploadTask(QObject *parent = 0);

    //Inputs
    void setImage(cwImage image);
    void setProjectFilename(QString filename);
    void setMipmapLevel(int level);

    //Outputs
    QList< QPair< QByteArray, QSize > > mipmaps() const;
    QVector2D scaleTexCoords() const;

    static bool isDivisibleBy4(QSize size);

protected:
    void runTask();

private:
    cwImage Image;
    QString ProjectFilename;
    int MipmapLevel = -1;

    QList< QPair< QByteArray, QSize > > Mipmaps;
    QVector2D ScaleTexCoords;

    void loadMipmapsFromDisk();

    void updateScaleTexCoords();

signals:
    
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








#endif // CWTEXTUREUPLOADTASK_H
