//Our includes
#include "cwTransformUpdater.h"
#include "cwDebug.h"

//Qt includes
#include <QDebug>

cwTransformUpdater::cwTransformUpdater(QObject *parent) :
    QObject(parent),
    Camera(NULL)
{

}

/**
  Sets the camera for the updater
  */
void cwTransformUpdater::setCamera(cwCamera* camera) {
    if(Camera != NULL) {
        disconnect(Camera, 0, this, 0);
    }

    Camera = camera;

    if(Camera != NULL) {
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
void cwTransformUpdater::addPointItem(QGraphicsObject* object) {
    if(object == NULL) { return; }

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
void cwTransformUpdater::removePointItem(QGraphicsObject* object) {
    if(!PointItems.contains(object)) {
        qDebug() << object << " isn't in the cwTransformUpdater, can't remove it" << LOCATION;
    }
    PointItems.remove(object);
}

/**
  Adds a item to the transform updater

  This is different then addPointItem.  Point item will change the object position without effecting
  it's scale.  This will create a 2D transform that will both position and scale the object.  This
  is useful for position lines and polygons in a two view.  This only work for 2d object, that there
  coordinates are defined in a opengl 2D cooridates system.  When transformUpdater calls update, the
  item's transformation will update.  The position will not be effeceted through setPos().
  */
void cwTransformUpdater::addTransformItem(QGraphicsObject* item) {
    if(item == NULL) { return; }

    if(TransformItems.contains(item)) {
        qDebug() << "Adding object twice in cwTransformUpdater " << LOCATION;
        return;
    }

    TransformItems.insert(item);
    connect(item, SIGNAL(destroyed(QObject*)), SLOT(transformItemDeleted(QObject*)));

    updateTransform(item);
}

/**
  This remove the item from the transformer
  */
void cwTransformUpdater::removeTransformItem(QGraphicsObject* item) {
    if(!TransformItems.contains(item)) {
        qDebug() << item << " isn't in the cwTransformUpdater, can't remove it" << LOCATION;
    }
    disconnect(item, NULL, this, NULL);
    TransformItems.remove(item);
}


/**
  This will update all the object's positions in the GL scene.

  This should be called when the camera has changed in anyway
  */
void cwTransformUpdater::update() {
    //Update transformation object
    updateTransformMatrix();

    //Update all the point objects
    foreach(QGraphicsObject* object, PointItems) {
        updatePoint(object);
    }

    //Update the transform objects
    foreach(QGraphicsObject* object, TransformItems) {
        updateTransform(object);
    }

    emit updated();
}

/**
  Update the points object.  This is different then updateTransform.  This updates the object's
  position using setPos.  This doesn't scale the object like updateTransform does.  This is
  useful for billboarded points.
  */
void cwTransformUpdater::updatePoint(QGraphicsObject* object) {
    QVector3D position = object->property("position3D").value<QVector3D>();
    QVector3D position2D = TransformMatrix * position;
    object->setPos(position2D.x(), position2D.y());
}

/**
  Update a transform object. This updates the object's position and scale based on setTransform.

  This is useful for updating lines or polygons, or anything that needs to keep it's shape.

  This assuems that the object's geometry has already been set in local 2D opengl coordinates.
  */
void cwTransformUpdater::updateTransform(QGraphicsObject* object) {
    Q_ASSERT(object != NULL);
    object->setTransform(TransformMatrix.toTransform());
}

/**
  This updates the transformer matrix that's used to transform all
  the objects to qt cooridantes
  */
void cwTransformUpdater::updateTransformMatrix() {
    if(Camera == NULL) { return; }
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
   QGraphicsObject* graphicsObject = qobject_cast<QGraphicsObject*>(object);
   removePointItem(graphicsObject);
}

/**
  This should only be called via a signal and slot connection

  This will update the position of a object, when it has changed.
  */
void cwTransformUpdater::handlePointItemDataChanged() {
    QGraphicsObject* item = qobject_cast<QGraphicsObject*>(sender());
    if(PointItems.contains(item)) {
        updatePoint(item);
    }
}

/**
  \brief Called when the object (a transform object) has been deleted

  This will remove the object from the transformUpdater
  */
void cwTransformUpdater::transformItemDeleted(QObject* object) {
    QGraphicsObject* graphicsObject = qobject_cast<QGraphicsObject*>(object);
    removeTransformItem(graphicsObject);
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


