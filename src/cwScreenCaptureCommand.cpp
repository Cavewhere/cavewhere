/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScreenCaptureCommand.h"
#include "cwScene.h"
#include "cwCamera.h"

//Qt includes
#include <QOpenGLFramebufferObject>
#include <QOPenGLFramebufferObjectFormat>


cwScreenCaptureCommand::cwScreenCaptureCommand() :
    Id(0)
{
}

void cwScreenCaptureCommand::setScene(cwScene *scene)
{
    Scene = scene;
}

void cwScreenCaptureCommand::setCamera(cwCamera *camera)
{
    Camera = camera;
}

/**
 * @brief cwScreenCaptureCommand::setId
 * @param id
 *
 * Sets the id for the screen capture.  This is useful for
 * multiple screencapture commands. The id isn't use for
 * any screencapture but is used for book keeping for
 * the caller.  The id is emited with the captured image.
 */
void cwScreenCaptureCommand::setId(int id)
{
    Id = id;
}

/**
 * @brief cwScreenCaptureCommand::excute
 */
void cwScreenCaptureCommand::excute()
{
    Q_ASSERT(!Camera.isNull());
    Q_ASSERT(!Scene.isNull());

    initializeOpenGLFunctions();

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);

    QSize size = Camera->viewport().size();

    Q_ASSERT(size.width() > 0);
    Q_ASSERT(size.height() > 0);

    cwCamera* oldCamera = Scene->camera();

    //Save the current framebuffer so we can rebind it
    GLint previousFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);

    //Paint the scene to a framebuffer object
    QOpenGLFramebufferObject framebuffer(size, format);
    framebuffer.bind();

    Scene->setCamera(Camera);
    Scene->paint();

    framebuffer.release();
    QImage image = framebuffer.toImage();

    Scene->setCamera(oldCamera);
    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);

    //Emit the created image
    emit createdImage(image, Id);
}
