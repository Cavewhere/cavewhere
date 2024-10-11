/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGETEXTURE_H
#define CWIMAGETEXTURE_H

//Qt include
#include <QObject>
#include <QFutureWatcher>
#include <QPair>
#include <QByteArray>
#include <QSize>
// #include <QGLFunctions>
#include <QOpenGLBuffer>
#include <QVector2D>

//Our includes
#include "cwImage.h"
#include "cwTextureUploadTask.h"
#include "cwFutureManagerToken.h"

class cwImageTexture : public QObject, private QOpenGLFunctions
{
    Q_OBJECT
    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QString project READ project WRITE setProject NOTIFY projectChanged)
    Q_PROPERTY(cwFutureManagerToken futureManagerToken READ futureManagerToken WRITE setFutureManagerToken NOTIFY futureManagerTokenChanged)

public:
    explicit cwImageTexture(QObject *parent = 0);
    ~cwImageTexture();

    void initialize();
    void releaseResources();

    void bind();
    void release();

    cwImage image() const;
    void setImage(cwImage image);

    QString project() const;
    void setProject(QString project);

    cwFutureManagerToken futureManagerToken() const;
    void setFutureManagerToken(cwFutureManagerToken futureManagerToken);

    QVector2D scaleTexCoords() const;

    bool isDirty() const;

signals:
    void projectChanged();
    void imageChanged();
    void textureUploaded();
    void needsUpdate();
    void futureManagerTokenChanged();

public slots:
    void updateData();

private:
    QString ProjectFilename; //!<
    cwImage Image; //!< The image that this texture represent
    QVector2D ScaleTexCoords; //!< How the texture should be scalede

    bool ReloadTexture; //If true reload the texture, delete and re-initilize
    bool TextureDirty; //!< true when the image needs to be updated
    bool DeleteTexture; //!< true when the image needs to be deleted
    GLuint TextureId; //!< Texture object

    cwTextureUploadTask::Format TextureType = cwTextureUploadTask::Unknown;

    QFuture<cwTextureUploadTask::UploadResult> UploadedTextureFuture;
    cwFutureManagerToken FutureManagerToken; //!<

    void deleteGLTexture();

    void setTextureType(cwTextureUploadTask::Format type);

    bool isImageValid(const cwImage& image) const;

private slots:
    void startLoadingImage();
    void markAsDirty();
};

/**
 * @brief cwImageTexture::isDirty
 * @return true if the texture out of date
 */
inline bool cwImageTexture::isDirty() const
{
    return TextureDirty;
}


/**
Gets project
*/
inline QString cwImageTexture::project() const {
    return ProjectFilename;
}

/**
  Gets image
  */
inline cwImage cwImageTexture::image() const {
    return Image;
}

/**
*
*/
inline cwFutureManagerToken cwImageTexture::futureManagerToken() const {
    return FutureManagerToken;
}

#endif // CWIMAGETEXTURE_H
