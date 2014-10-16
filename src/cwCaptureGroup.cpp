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
    connect(PositionMapper, SIGNAL(mapped(QObject*)), this, SLOT(updateTranslationFrom(QObject*)));
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

    CaptureData.insert(capture, ViewportGroupData());

    if(!Captures.isEmpty()) {
        updateCaptureScale(primaryCapture(), capture);
        updateCaptureRotation(primaryCapture(), capture);
        initializePosition(capture);
    }

    Captures.append(capture);
    updateViewportGroupData(capture);

    ScaleMapper->setMapping(capture->scaleOrtho(), capture);
    RotationMapper->setMapping(capture, capture);
    PositionMapper->setMapping(capture, capture);

    connect(capture->scaleOrtho(), SIGNAL(scaleChanged()), ScaleMapper, SLOT(map()));
    connect(capture, SIGNAL(rotationChanged()), RotationMapper, SLOT(map()));
    connect(capture, SIGNAL(positionOnPaperChanged()), PositionMapper, SLOT(map()));
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
    disconnect(capture, SIGNAL(positionOnPaperChanged()), PositionMapper, SLOT(map()));

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

    disconnect(captureToUpdate->scaleOrtho(), SIGNAL(scaleChanged()), ScaleMapper, SLOT(map()));

    double newScale = fixedCapture->scaleOrtho()->scale();
    captureToUpdate->scaleOrtho()->setScale(newScale);

    updateViewportGroupData(captureToUpdate);

    connect(captureToUpdate->scaleOrtho(), SIGNAL(scaleChanged()), ScaleMapper, SLOT(map()));
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

    updateViewportGroupData(captureToUpdate);

    connect(RotationMapper, SIGNAL(mapped(QObject*)), this, SLOT(updateRotationFrom(QObject*)));
}

/**
 * @brief cwCaptureGroup::updateCatpureTranslation
 * @param fixedCapture
 * @param captureToUpdate
 *
 * This updates the captureToUpdate with the fixedCapture's translation
 */
void cwCaptureGroup::updateCaptureTranslation(cwCaptureViewport *capture)
{
    Q_ASSERT(!Captures.isEmpty());
    if(capture == primaryCapture()) { return; }

    //Only update the capture translation, if the pitch doesn't match
    if(isCoplanerWithPrimaryCapture(capture)) {

        disconnect(capture, SIGNAL(positionOnPaperChanged()), PositionMapper, SLOT(map()));

        QPointF mappedOrigin = capture->mapToCapture(primaryCapture());
        QPointF currentPos = capture->positionOnPaper();

        QTransform transform;
        transform.rotate(-capture->rotation());
        transform.translate(-mappedOrigin.x(), -mappedOrigin.y());
        QPointF mappedPos = transform.map(currentPos);
        mappedPos.setX(0);

        QTransform invertedTransorm = transform.inverted();
        QPointF newPosition = invertedTransorm.map(mappedPos);

        capture->setPositionOnPaper(newPosition);

        updateViewportGroupData(capture);

        connect(capture, SIGNAL(positionOnPaperChanged()), PositionMapper, SLOT(map()));
    }
}

/**
 * @brief cwCaptureGroup::initializePosition
 * @param capture
 *
 * This initialize's the position of the capture. The capture shouldn't be the first capture
 * in the group.
 */
void cwCaptureGroup::initializePosition(cwCaptureViewport *capture)
{
    Q_ASSERT(!Captures.isEmpty());
    Q_ASSERT(primaryCapture() != capture);
    QPointF newOrigin = capture->mapToCapture(primaryCapture());
    capture->setPositionOnPaper(newOrigin);
    updateViewportGroupData(capture);
}

/**
 * @brief cwCaptureGroup::updateViewportGroupData
 * @param capture
 *
 * This updates the viewport's group data with catpure's data. This will update
 * the OldPostion and Offset in the ViewportGroupData.
 *
 * If the capture is the primary capture. Then this only update the OldPostion.
 * If the capture is in the same plan as the primary. This will only update
 * the OldPosition.
 */
void cwCaptureGroup::updateViewportGroupData(cwCaptureViewport *capture)
{
    Q_ASSERT(CaptureData.contains(capture));

    CaptureData[capture].OldPosition = capture->positionOnPaper();

    if(capture != primaryCapture() && !isCoplanerWithPrimaryCapture(capture)) {
        //A non-planar catpure, update the offset
        QPointF origin = capture->mapToCapture(primaryCapture());
        QPointF currentPosition = capture->positionOnPaper();
        double offset = QLineF(origin, currentPosition).length();

        CaptureData[capture].Offset = offset;
    }
}

/**
 * @brief cwCaptureGroup::primaryCapture
 * @return Returns the primaryCapture
 *
 * By default, this is the first item in the group.
 */
cwCaptureViewport *cwCaptureGroup::primaryCapture() const
{
    return Captures.first();
}

/**
 * @brief cwCaptureGroup::isCoplanerWithPrimaryCapture
 * @param capture
 * @return True if the capture is coplaner with the primaryCapture (returns true if pitch's are
 * equal between capture and primaryCapture)
 */
bool cwCaptureGroup::isCoplanerWithPrimaryCapture(cwCaptureViewport *capture) const
{
    return capture->cameraPitch() != primaryCapture()->cameraPitch();
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
        updateCaptureTranslation(current);
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
    Q_ASSERT(!Captures.isEmpty());
    Q_ASSERT(dynamic_cast<cwCaptureViewport*>(capture) != nullptr);
    cwCaptureViewport* fixedCapture = static_cast<cwCaptureViewport*>(capture);

    if(fixedCapture == primaryCapture()) {
        //This is the primary catpure. It has move, simply offset based on it's previous Position
        QPointF offset = fixedCapture->positionOnPaper() - CaptureData[fixedCapture].OldPosition; //OldPrimaryPosition;
        foreach(cwCaptureViewport* current, Captures) {
            if(fixedCapture != current) {
                QPointF oldPosition = current->positionOnPaper();
                current->setPositionOnPaper(oldPosition + offset);
            }
        }
        updateViewportGroupData(fixedCapture);
    } else {
        //This is a secondard capture. Update it show it aligns with it's rotation
        updateCaptureTranslation(fixedCapture);
    }
}
