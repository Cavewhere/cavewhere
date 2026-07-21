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
#include <QMetaProperty>
#include <QSGTransformNode>

namespace {
    //The 3D position the updater reprojects into Qt view coordinates every frame.
    constexpr auto kPosition3DProperty = "position3D";

    //Managed items compose this into their own visible binding. The updater used to
    //write visible directly, which silently fought any binding the item's owner put
    //on it: a C++ setter does not remove a QML binding, so the two writers raced and
    //the binding won every time it re-evaluated, defeating the cull.
    constexpr auto kInFrustumProperty = "inFrustum";

    //! The metaobject index of a named property, or -1 when absent (or, if
    //! requireWritable, present but read-only). Resolved once per item so the hot
    //! per-frame path reads/writes by index instead of by name.
    int propertyIndex(const QQuickItem* object, const char* name, bool requireWritable) {
        const int index = object->metaObject()->indexOfProperty(name);
        if(index < 0) {
            return -1;
        }
        if(requireWritable && !object->metaObject()->property(index).isWritable()) {
            return -1;
        }
        return index;
    }
}

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
        connect(Camera, SIGNAL(viewMatrixChanged()), SLOT(update()));
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

    //Resolved once here rather than per frame in updatePoint, which runs for every
    //point on every camera change.
    const PointIndices indices {
        propertyIndex(object, kPosition3DProperty, false),
        propertyIndex(object, kInFrustumProperty, true)
    };
    if(indices.inFrustum < 0) {
        qWarning() << object << "has no writable `inFrustum` property, so it won't be hidden"
                   << "when it leaves the viewport" << LOCATION;
    }

    PointItems.insert(object, indices);
    connect(object, &QQuickItem::destroyed, this, &cwTransformUpdater::pointItemDeleted);
    connect(object, SIGNAL(position3DChanged()), this, SLOT(handlePointItemDataChanged()));

    if(Camera) {
        updatePoint(object, indices);
    }
}

/**
  Removes a object from the transform updater
  */
void cwTransformUpdater::removePointItem(QQuickItem *object) {
    if(object == nullptr) { return; }

    if(!PointItems.contains(object)) {
        qDebug() << object << " isn't in the cwTransformUpdater, can't remove it" << LOCATION;
        return;
    }

    disconnect(object, nullptr, this, nullptr);
    PointItems.remove(object);
}



/**
  This will update all the object's positions in the GL scene.

  This should be called when the camera has changed in anyway
  */
void cwTransformUpdater::update() {
    if(enabled()
        && Camera != nullptr)
    {
        //Update transformation object
        updateTransformMatrix();

        //Update all the point objects. Iterate a copy: updatePoint writes properties
        //that can synchronously run QML handlers, and one that adds or removes a managed
        //item would invalidate a live iterator. QHash is implicitly shared, so this is a
        //refcount bump unless a mutation actually happens mid-iteration - the safety the
        //old foreach provided for free.
        const auto pointItems = PointItems;
        for(auto it = pointItems.cbegin(); it != pointItems.cend(); ++it) {
            updatePoint(it.key(), it.value());
        }

        emit updated();
    }
}

/**
  Update the points object.  This is different then updateTransform.  This updates the object's
  position using setPos.  This doesn't scale the object like updateTransform does.  This is
  useful for billboarded points.
  */
void cwTransformUpdater::updatePoint(QQuickItem *object, PointIndices indices) {
    Q_ASSERT(Camera != nullptr);
    Q_ASSERT(object != nullptr);

    const QVector3D position = indices.position3D >= 0
            ? object->metaObject()->property(indices.position3D).read(object).value<QVector3D>()
            : QVector3D();
    const QVector3D position2D = TransformMatrix.map(position);

    //Publish frustum membership instead of writing visible: the item's owner decides
    //what to do with it, so a delegate can have its own reasons to be hidden without
    //the two writers overwriting each other. Positive polarity keeps owner bindings
    //an AND (`visible: inFrustum && ...`) rather than a negation.
    if(indices.inFrustum >= 0) {
        const bool inFrustum = !Camera->isQtViewportCoordinateClipped(position2D);
        object->metaObject()->property(indices.inFrustum).write(object, inFrustum);
    }

    object->setPosition(QPointF(position2D.x(), position2D.y()));
}

/**
  This updates the transformer matrix that's used to transform all
  the objects to qt cooridantes
  */
void cwTransformUpdater::updateTransformMatrix() {
    Q_ASSERT(Camera != nullptr);
    TransformMatrix = Camera->qtViewportMatrix() * Camera->viewProjectionMatrix() * ModelMatrix;

    emit matrixChanged();
}

/**
  \brief Called when the object (a point object) has been deleted

  This will remove the object from the transformUpdater
  */
void cwTransformUpdater::pointItemDeleted(QObject* object) {
    //Object has already been deleted, we need to remove it directly
    //dynamic_cast don't work because the subclass has had destructor called already
    PointItems.remove(static_cast<QQuickItem*>(object));

    //Don't call removePointItem, object has already been partcially deleted
    // QQuickItem* graphicsObject = qobject_cast<QQuickItem*>(object);
    // removePointItem(graphicsObject);
}

/**
  This should only be called via a signal and slot connection

  This will update the position of a object, when it has changed.
  */
void cwTransformUpdater::handlePointItemDataChanged() {
    if (Camera == nullptr) {
        return;
    }

    QQuickItem* item = qobject_cast<QQuickItem*>(sender());
    auto it = PointItems.constFind(item);
    if(it != PointItems.cend()) {
        updatePoint(it.key(), it.value());
    }
}

/**
  Maps the viewport point into the local position
  */
QVector3D cwTransformUpdater::mapFromViewportToModel(QPointF viewport) const {
    return matrix().inverted().map(QVector3D(viewport));
}

/**
  Maps a 3d model point into a viewport position
  */
QPointF cwTransformUpdater::mapModelToViewport(QVector3D modelPoint) const {
    QVector3D viewportPoint = matrix().map(modelPoint);
    return viewportPoint.toPointF();
}

/**
*
*/
void cwTransformUpdater::setEnabled(bool enabled) {
    if(Enabled != enabled) {
        Enabled  = enabled;
        update();
        emit enabledChanged();
    }
}
