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
#include "cwMappedQImage.h"

//Qt includes
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QFile>
#include <QUuid>
#include <QDir>

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

    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    //Paint the scene to a framebuffer object
    QOpenGLFramebufferObject framebuffer(size, format);
    framebuffer.bind();

    glViewport(0, 0, size.width(), size.height());

    Scene->setCamera(Camera);
    Scene->paint();

    framebuffer.release();
    QImage image = cwMappedQImage::createDiskImageWithTempFile(QString("cw-capture-%1").arg(Id), framebuffer.toImage());

    Scene->setCamera(oldCamera);
    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);

    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

    //Emit the created image
    emit createdImage(image, Id);
}
