/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMAGEITEM_H
#define CWIMAGEITEM_H

//Qt includes
#include <QString>
#include <QFutureWatcher>

//Our includes
#include "cwGLRenderer.h"
#include "cwImage.h"
#include "cwImageData.h"
class cwGLImageItemResources;
class cwImageTexture;
class cwImageProperties;

class cwImageItem : public cwGLRenderer
{
    Q_OBJECT

    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QString projectFilename READ projectFilename WRITE setProjectFilename NOTIFY projectFilenameChanged())
    Q_PROPERTY(float rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(QMatrix4x4 modelMatrix READ modelMatrix NOTIFY modelMatrixChanged)
    Q_PROPERTY(cwImageProperties* imageProperties READ imageProperties NOTIFY imagePropertiesChanged)

public:
    cwImageItem(QQuickItem *parent = 0);
    ~cwImageItem();

    Q_INVOKABLE void clearImage();
    void setImage(const cwImage& image);
    cwImage image() const;

    cwImageProperties* imageProperties() const;

    void setRotation(float degrees);
    float rotation() const;

    QString projectFilename() const;
    void setProjectFilename(QString projectFilename);

    QMatrix4x4 modelMatrix() const;

    Q_INVOKABLE void updateRotationCenter();

    Q_INVOKABLE QPointF mapQtViewportToNote(QPoint qtViewportCoordinate);
    Q_INVOKABLE QPointF mapNoteToQtViewport(QPointF mapNote) const;


signals:
    void rotationChanged();
    void imageChanged();
    void projectFilenameChanged();
    void modelMatrixChanged();
    void imagePropertiesChanged();

protected:
    virtual void initializeGL();
    virtual void resizeGL();
    virtual void paint(QPainter* painter);
    virtual void releaseResources();

    virtual QSGNode * updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *data);

private:

    //The project filename for this class
    cwImage Image;
    cwImageProperties* ImageProperties; //!< The image properties of the current Image
    float Rotation;
    //QString ProjectFilename;
    QMatrix4x4 RotationModelMatrix;
    QPointF RotationCenter;
    QString ProjectFilename;

    //For rendering
    cwGLImageItemResources* GLResources;
    static int vVertex; //!< The attribute location of the vVertex
    static int ModelViewProjectionMatrix; //!< The uniform location for modelViewProjection matrix
    static int CropAreaUniform; //!< The uniform location of CropArea this is for trimming padding of the images
//    cwImageTexture* NoteTexture;
    static QOpenGLShaderProgram* ImageProgram; //!< The image shader program that's used to render the image
//    QOpenGLBuffer GeometryVertexBuffer; //!< The vertex buffer

    void initializeShaders();
    void initializeVertexBuffers();
    void initializeTexture();

private slots:
     void imageFinishedLoading();

};

/**
  Gets the image
  */
inline cwImage cwImageItem::image() const {
    return Image;
}

/**
  Gets the rotation of the image in degress.
  */
inline float cwImageItem::rotation() const {
    return Rotation;
}

/**
  \brief Gets the model matrix
  */
inline QMatrix4x4 cwImageItem::modelMatrix() const {
    return RotationModelMatrix;
}

/**
Gets imageProperties
*/
inline cwImageProperties* cwImageItem::imageProperties() const {
    return ImageProperties;
}

#endif // CWIMAGEITEM_H
