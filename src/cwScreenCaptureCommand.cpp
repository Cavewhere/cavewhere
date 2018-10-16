/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScreenCaptureCommand.h"
#include "cwCamera.h"

cwScreenCaptureCommand::cwScreenCaptureCommand(QObject *parent) :
    QObject(parent)
{
}

cwScreenCaptureCommand::~cwScreenCaptureCommand()
{
    delete Camera;
}

/**
* @brief cwScreenCaptureCommand::setCamera
*
* Sets the camera of the command. The command will take ownership of the command
*/
void cwScreenCaptureCommand::setCamera(cwCamera* camera) {
    if(Camera != camera) {
        Camera = camera;
        emit cameraChanged();
    }
}

/**
* @brief cwScreenCaptureCommand::setOrigin
*
* Sets the origin of the image that the screen capture will take
*/
void cwScreenCaptureCommand::setOrigin(QPointF origin) {
    if(Origin != origin) {
        Origin = origin;
        emit originChanged();
    }
}

/**
* @brief cwScreenCaptureCommand::setImage
*
* Sets the image that's capture when the command is executed
*/
void cwScreenCaptureCommand::setImage(QImage image) {
    if(Image != image) {
        Image = image;
        emit imageChanged();
    }
}

/**
* @brief cwScreenCaptureCommand::camera
*
* Returns the camera of the command
*/
cwCamera* cwScreenCaptureCommand::camera() const {
    return Camera;
}


