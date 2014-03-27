/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwCaptureSceneCommand.h"
#include "cwScene.h"
#include "cwCamera.h"

cwCaptureSceneCommand::cwCaptureSceneCommand(QObject *parent) :
    cwSceneCommand(parent)
{
}

/**
 * @brief cwCaptureSceneCommand::setScene
 * @param scene
 */
void cwCaptureSceneCommand::setScene(cwScene *scene)
{
   Scene = scene;
}

/**
 * @brief cwCaptureSceneCommand::scene
 * @return
 */
cwScene *cwCaptureSceneCommand::scene() const
{
    return Scene;
}

/**
 * @brief cwCaptureSceneCommand::setCamera
 * @param camera
 */
void cwCaptureSceneCommand::setCamera(cwCamera *camera)
{
    Camera = camera;
}

/**
 * @brief cwCaptureSceneCommand::camera
 * @return
 */
cwCamera *cwCaptureSceneCommand::camera() const
{
    return Camera;
}

/**
 * @brief cwCaptureSceneCommand::excute
 *
 * This creates a framebuffer and renders the scene to the framebuffer.
 */
void cwCaptureSceneCommand::excute()
{
    initializeOpenGLFunctions();

    //Initilizes and binds the framebuffer that we're rendering to
    initilizeFramebuffer();

    cwCamera* oldCamera = Scene->camera();
    Scene->setCamera(camera());

    //Paint the scene to the currently bound framebuffer
    Scene->paint();

    //Return the scene back to original state so normal rendering can continue
    Scene->setCamera(oldCamera);
    glBindFramebuffer(GL_FRAMEBUFFER, OldFramebufferObject);

}

void cwCaptureSceneCommand::initilizeFramebuffer()
{
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &OldFramebufferObject);

    //Create the framebuffer that we're going to render to
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    FramebufferObject = new QOpenGLFramebufferObject(camera()->viewport().size(), format);
    FramebufferObject->bind();
}
