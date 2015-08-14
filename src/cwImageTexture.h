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
#include <QGLFunctions>
#include <QOpenGLBuffer>
#include <QVector2D>

//Our includes
#include "cwImage.h"
class cwTextureUploadTask;

class cwImageTexture : public QObject, private QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit cwImageTexture(QObject *parent = 0);
    ~cwImageTexture();

    Q_PROPERTY(QString project READ project WRITE setProject NOTIFY projectChanged)
    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)

    void initialize();

    void bind();
    void release();

    cwImage image() const;
    void setImage(cwImage image);

    QString project() const;
    void setProject(QString project);

    QVector2D scaleTexCoords() const;

    bool isDirty() const;

signals:
    void projectChanged();
    void imageChanged();
    void textureUploaded();

public slots:
    void updateData();

private:
    QString ProjectFilename; //!<
    cwImage Image; //!< The image that this texture represent
    QVector2D ScaleTexCoords; //!< How the texture should be scalede

    bool TextureDirty; //!< true when the image needs to be updated
    bool DeleteTexture; //!< true when the image needs to be deleted
    GLuint TextureId; //!< Texture object

    cwTextureUploadTask* TextureUploadTask;

    void deleteLoadNoteTask();
    void deleteGLTexture();

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

///**
//  This binds the texture to current texture unit
//  */
//inline void cwImageTexture::bind() {
//    glActiveTexture(GL_TEXTURE0);
//    glBindTexture(GL_TEXTURE_2D, TextureId);
//}

///**
//    Releases the texture
//  */
//inline void cwImageTexture::release() {
//    glBindTexture(GL_TEXTURE_2D, 0);
//}

#endif // CWIMAGETEXTURE_H
