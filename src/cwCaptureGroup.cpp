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
    }

    Captures.append(capture);

    ScaleMapper->setMapping(capture->scaleOrtho(), capture);
    connect(capture->scaleOrtho(), SIGNAL(scaleChanged()), ScaleMapper, SLOT(map()));
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
    disconnect(capture->scaleOrtho(), SIGNAL(scaleChanged()), ScaleMapper, SLOT(map()));

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
    double newScale = fixedCapture->scaleOrtho()->scale();
    captureToUpdate->scaleOrtho()->setScale(newScale);
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


