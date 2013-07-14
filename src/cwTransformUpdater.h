/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRANSFORMUPDATER_H
#define CWTRANSFORMUPDATER_H

//Qt includes
#include <QObject>
#include <QMatrix4x4>
#include <QQuickItem>

//Our includes
#include "cwCamera.h"

/**
  \brief This class will watch a cwCamera

  When the camera is updated, this will update all the child objects with a new
  tranformation matrix.  The transformation matrix maps the child object's position (in local model
  coordinates) into Qt view coordinates.  This class is extremely useful for mapping 3d positions into
  2D qt view coordinates.  This class will automatically, update the child graphics object positions.

  All items that are added to the transform need to have "position" property that's QVector3D.
  */
class cwTransformUpdater : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QMatrix4x4 modelMatrix READ modelMatrix WRITE setModelMatrix NOTIFY matrixChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix NOTIFY matrixChanged)

public:
    explicit cwTransformUpdater(QObject *parent = 0);

     void setCamera(cwCamera* camera);
    cwCamera* camera() const;

    void setModelMatrix(QMatrix4x4 matrix);
    QMatrix4x4 modelMatrix() const;

    void addPointItem(QQuickItem* object);
    void removePointItem(QQuickItem* object);

    QMatrix4x4 matrix() const;

    Q_INVOKABLE QVector3D mapFromViewportToModel(QPointF viewport) const;
    Q_INVOKABLE QPointF mapModelToViewport(QVector3D modelPoint) const;
    Q_INVOKABLE QPointF mapModelToViewport(QPointF modelPoint) const;

signals:
    void matrixChanged();
    void cameraChanged();
    void updated();

public slots:
    void update();

private slots:
    void pointItemDeleted(QObject* object);
    void handlePointItemDataChanged();

private:
    QSet<QQuickItem*> PointItems;
    cwCamera* Camera;
    QMatrix4x4 ModelMatrix;

    QMatrix4x4 TransformMatrix; //!< The total matrix that converts a object's position into qt coordinates

    void updatePoint(QQuickItem* object);

    void updateTransformMatrix();

};

/**
  \brief Gets the matrix that can transform a point from GL point to Qt item coordinates

  This will use the camera's projection, view, and viewport and the modelMatrix to do the transformation
  */
inline QMatrix4x4 cwTransformUpdater::matrix() const {
    return TransformMatrix;
}


/**
  Maps modelPoint to into a viewport position
  */
inline QPointF cwTransformUpdater::mapModelToViewport(QPointF modelPoint) const
{
    return mapModelToViewport(QVector3D(modelPoint));
}


/**
  \brief Get's the camera for the transform updater
  */
inline cwCamera* cwTransformUpdater::camera() const {
    return Camera;
}

/**
  Set the model matrix of the items. All the items need have a position property in a local QVector3D
  cooridant system.
  */
inline QMatrix4x4 cwTransformUpdater::modelMatrix() const {
    return ModelMatrix;
}

#endif // CWTRANSFORMUPDATER_H
