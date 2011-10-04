#ifndef CWTRANSFORMUPDATER_H
#define CWTRANSFORMUPDATER_H

//Qt includes
#include <QObject>
#include <QMatrix4x4>
#include <QGraphicsObject>

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
public:
    explicit cwTransformUpdater(QObject *parent = 0);

    void setCamera(cwCamera* camera);
    cwCamera* camrea() const;

    void setModelMatrix(QMatrix4x4 matrix);
    QMatrix4x4 modelMatrix() const;

    void addPointItem(QGraphicsObject* object);
    void removePointItem(QGraphicsObject* object);

    void addTransformItem(QGraphicsObject* item);
    void removeTransformItem(QGraphicsObject* item);

    QMatrix4x4 matrix() const;

signals:
    void matrixChanged();

public slots:
    void update();
    void updateTransform(QGraphicsObject* object);

private slots:
    void pointItemDeleted(QObject* object);
    void handlePointItemDataChanged();

    void transformItemDeleted(QObject* object);
private:
    QSet<QGraphicsObject*> PointItems;
    QSet<QGraphicsObject*> TransformItems;
    cwCamera* Camera;
    QMatrix4x4 ModelMatrix;

    QMatrix4x4 TransformMatrix; //!< The total matrix that converts a object's position into qt coordinates

    void updatePoint(QGraphicsObject* object);

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
  Set the model matrix of the items. All the items need have a position property in a local QVector3D
  cooridant system.
  */
inline QMatrix4x4 cwTransformUpdater::modelMatrix() const {
    return ModelMatrix;
}

#endif // CWTRANSFORMUPDATER_H
