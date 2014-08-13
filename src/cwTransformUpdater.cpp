/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTransformUpdater.h"
#include "cwDebug.h"

//Qt includes
#include <QDebug>
#include <QSGTransformNode>

cwTransformUpdater::cwTransformUpdater(QObject *parent) :
    QObject(parent),
    Camera(nullptr)
{

}

/**
  Sets the camera for the updater
  */
void cwTransformUpdater::setCamera(cwCamera* camera) {
    if(Camera != nullptr) {
        disconnect(Camera, 0, this, 0);
    }

    Camera = camera;

    if(Camera != nullptr) {
        connect(Camera, SIGNAL(projectionChanged()), SLOT(update()));
        connect(Camera, SIGNAL(viewChanged()), SLOT(update()));
        connect(Camera, SIGNAL(viewportChanged()), SLOT(update()));
        update();

        emit cameraChanged();
    }
}

/**
  Updates the model matrix
  */
void cwTransformUpdater::setModelMatrix(QMatrix4x4 matrix) {
    ModelMatrix = matrix;
    update();
}

/**
  Adds a addItem to the graphics object

  This will update the object position by using setPosition.  The object needs to have "position3D"
  property for this to work.
  */
void cwTransformUpdater::addPointItem(QQuickItem *object) {
    if(object == nullptr) { return; }

    if(PointItems.contains(object)) {
        qDebug() << "Adding object twice in cwTransformUpdater " << LOCATION;
        return;
    }

    PointItems.insert(object);
    connect(object, SIGNAL(destroyed(QObject*)), SLOT(pointItemDeleted(QObject*)));
    connect(object, SIGNAL(position3DChanged()), SLOT(handlePointItemDataChanged()));
    updatePoint(object);
}

/**
  Removes a object from the transform updater
  */
void cwTransformUpdater::removePointItem(QQuickItem *object) {
    if(object == nullptr) { return; }

    if(!PointItems.contains(object)) {
        qDebug() << (void*)object << " isn't in the cwTransformUpdater, can't remove it" << LOCATION;
    }
    PointItems.remove(object);
}



/**
  This will update all the object's positions in the GL scene.

  This should be called when the camera has changed in anyway
  */
void cwTransformUpdater::update() {
    //Update transformation object
    updateTransformMatrix();

    //Update all the point objects
    foreach(QQuickItem* object, PointItems) {
        updatePoint(object);
    }

    emit updated();
}

/**
  Update the points object.  This is different then updateTransform.  This updates the object's
  position using setPos.  This doesn't scale the object like updateTransform does.  This is
  useful for billboarded points.
  */
void cwTransformUpdater::updatePoint(QQuickItem *object) {
    QVector3D position = object->property("position3D").value<QVector3D>();
    QVector3D position2D = TransformMatrix * position;
    object->setPosition(QPointF(position2D.x(), position2D.y()));
}

/**
  This updates the transformer matrix that's used to transform all
  the objects to qt cooridantes
  */
void cwTransformUpdater::updateTransformMatrix() {
    if(Camera == nullptr) { return; }
    float width = Camera->viewport().width();
    float height = Camera->viewport().height();

    QMatrix4x4 qtViewportMatrix;
    qtViewportMatrix.scale(width / 2.0, -height / 2.0, 1.0);
    qtViewportMatrix.translate(1.0, -1.0, 0.0);

    TransformMatrix = qtViewportMatrix * Camera->viewProjectionMatrix() * ModelMatrix;

    emit matrixChanged();
}

/**
  \brief Called when the object (a point object) has been deleted

  This will remove the object from the transformUpdater
  */
void cwTransformUpdater::pointItemDeleted(QObject* object) {
   QQuickItem* graphicsObject = qobject_cast<QQuickItem*>(object);
   removePointItem(graphicsObject);
}

/**
  This should only be called via a signal and slot connection

  This will update the position of a object, when it has changed.
  */
void cwTransformUpdater::handlePointItemDataChanged() {
    QQuickItem* item = qobject_cast<QQuickItem*>(sender());
    if(PointItems.contains(item)) {
        updatePoint(item);
    }
}

/**
  Maps the viewport point into the local position
  */
QVector3D cwTransformUpdater::mapFromViewportToModel(QPointF viewport) const {
    return matrix().inverted() * QVector3D(viewport);
}

/**
  Maps a 3d model point into a viewport position
  */
QPointF cwTransformUpdater::mapModelToViewport(QVector3D modelPoint) const {
    QVector3D viewportPoint = matrix() * modelPoint;
    return viewportPoint.toPointF();
}


