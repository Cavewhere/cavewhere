/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwCaptureGroup.h"
#include "cwCaptureViewport.h"
#include "cwScale.h"

cwCaptureGroup::cwCaptureGroup(QObject *parent) :
    QObject(parent),
    ScaleMapper(new QSignalMapper(this)),
    PositionMapper(new QSignalMapper(this)),
    RotationMapper(new QSignalMapper(this))
{
    connect(ScaleMapper, SIGNAL(mapped(QObject*)), this, SLOT(updateScalesFrom(QObject*)));
    connect(RotationMapper, SIGNAL(mapped(QObject*)), this, SLOT(updateRotationFrom(QObject*)));
}

/**
 * @brief cwCaptureGroup::addCapture
 * @param capture
 *
 * Addes a capture to the group.
 * When adding a capture, this group doesn't own the capture. The cwCaptureManager does.
 *
 * The catpure most have an othognal projection.
 *
 * This function will assert if capture is nullptr
 * This function will assert if caputer already exists
 */
void cwCaptureGroup::addCapture(cwCaptureViewport *capture)
{
    Q_ASSERT(capture != nullptr);
    Q_ASSERT(!contains(capture));

    if(!Captures.isEmpty()) {
        updateCaptureScale(Captures.first(), capture);
        updateCaptureRotation(Captures.first(), capture);
        updateCaptureTranslation(Captures.first(), capture);
    }

    Captures.append(capture);

    ScaleMapper->setMapping(capture->scaleOrtho(), capture);
    RotationMapper->setMapping(capture, capture);

    connect(capture->scaleOrtho(), SIGNAL(scaleChanged()), ScaleMapper, SLOT(map()));
    connect(capture, SIGNAL(rotationChanged()), RotationMapper, SLOT(map()));
}

/**
 * @brief cwCaptureGroup::removeCapture
 * @param capture
 *
 * This will go through the group and remove capture from it.
 */
void cwCaptureGroup::removeCapture(cwCaptureViewport *capture)
{
    Q_ASSERT(capture != nullptr);
    Q_ASSERT(contains(capture));

    ScaleMapper->removeMappings(capture->scaleOrtho());
    RotationMapper->removeMappings(capture);

    disconnect(capture->scaleOrtho(), SIGNAL(scaleChanged()), ScaleMapper, SLOT(map()));
    disconnect(capture, SIGNAL(rotationChanged()), RotationMapper, SLOT(map()));

    Captures.removeOne(capture);
}



/**
 * @brief cwCaptureGroup::updateCapture
 * @param fixedCapture - The capture that won't changed
 * @param captureToUpdate - The capture that will be updated
 *
 * This will update the captureToUpdate with the fixedCapture's data. The catpureToUpdate
 * scale, position, and rotation will be updated.
 */
void cwCaptureGroup::updateCaptureScale(const cwCaptureViewport *fixedCapture,
                                   cwCaptureViewport *captureToUpdate)
{
    if(fixedCapture == captureToUpdate) { return; }
    double newScale = fixedCapture->scaleOrtho()->scale();
    captureToUpdate->scaleOrtho()->setScale(newScale);
}

/**
 * @brief cwCaptureGroup::updateCaptureRotation
 * @param fixedCapture
 * @param captureToUpdate
 */
void cwCaptureGroup::updateCaptureRotation(const cwCaptureViewport *fixedCapture,
                                           cwCaptureViewport *captureToUpdate)
{
    if(fixedCapture == captureToUpdate) { return; }

    disconnect(RotationMapper, SIGNAL(mapped(QObject*)), this, SLOT(updateRotationFrom(QObject*)));

    double fixedRotation = fixedCapture->cameraAzimuth() - fixedCapture->rotation();
    double captureRotation = captureToUpdate->cameraAzimuth() - captureToUpdate->rotation();
    double diffRotation =   captureRotation - fixedRotation;

    double oldRotation = captureToUpdate->rotation();
    captureToUpdate->setRotation(oldRotation + diffRotation);

    connect(RotationMapper, SIGNAL(mapped(QObject*)), this, SLOT(updateRotationFrom(QObject*)));
}

/**
 * @brief cwCaptureGroup::updateCatpureTranslation
 * @param fixedCapture
 * @param captureToUpdate
 *
 * This updates the captureToUpdate with the fixedCapture's translation
 */
void cwCaptureGroup::updateCaptureTranslation(const cwCaptureViewport *fixedCapture, cwCaptureViewport *captureToUpdate)
{
//    QPointF origin = fixedCapture->mapToCapture(WorldToPaper(QVector3D(0.0, 0.0, 0.0));
    QPointF updateOrigin = captureToUpdate->mapToCapture(fixedCapture);

    QPointF oldPosition = captureToUpdate->positionOnPaper();
    QPointF newPosition = updateOrigin - oldPosition;

    qDebug() << "OldPosition:" << oldPosition << newPosition << updateOrigin;

    captureToUpdate->setPositionOnPaper(updateOrigin);
}

/**
 * @brief cwCaptureGroup::updateScales
 * @param capture
 *
 * This updates all the scales in this group to match capture's scale.
 */
void cwCaptureGroup::updateScalesFrom(QObject *capture)
{
    Q_ASSERT(dynamic_cast<cwCaptureViewport*>(capture) != nullptr);
    cwCaptureViewport* fixedCapture = static_cast<cwCaptureViewport*>(capture);

    foreach(cwCaptureViewport* current, Captures) {
        updateCaptureScale(fixedCapture, current);
    }
}

/**
 * @brief cwCaptureGroup::updateRotationFor
 * @param capture
 *
 * This updates all the rotations in this group to match the capture's translation
 */
void cwCaptureGroup::updateRotationFrom(QObject *capture)
{
    Q_ASSERT(dynamic_cast<cwCaptureViewport*>(capture) != nullptr);
    cwCaptureViewport* fixedCapture = static_cast<cwCaptureViewport*>(capture);

    foreach(cwCaptureViewport* current, Captures) {
        updateCaptureRotation(fixedCapture, current);
    }
}

/**
 * @brief cwCaptureGroup::updateTranslationFrom
 * @param capture
 *
 * This will update all the translation in this group to match the capture's translation.
 *
 * This will also constain the capture position, so that it can only be move in parallel
 * to it's rotation.
 */
void cwCaptureGroup::updateTranslationFrom(QObject *capture)
{
    Q_ASSERT(dynamic_cast<cwCaptureViewport*>(capture) != nullptr);
    cwCaptureViewport* fixedCapture = static_cast<cwCaptureViewport*>(capture);

    foreach(cwCaptureViewport* current, Captures) {
        updateCaptureTranslation(fixedCapture, current);
    }
}


