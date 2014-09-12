/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwScene.h"
#include "cwGLObject.h"
#include "cwSceneCommand.h"
#include "cwShaderDebugger.h"
#include "cwInitializeOpenGLFunctionsCommand.h"

cwScene::cwScene(QObject *parent) :
    QObject(parent),
    GeometryItersecter(new cwGeometryItersecter()),
    ShaderDebugger(new cwShaderDebugger(this)),
    Camera(nullptr),
    ExcutingCommands(false)
{
    cwInitializeOpenGLFunctionsCommand* initOpenGLFunctionCommand = new cwInitializeOpenGLFunctionsCommand();
    initOpenGLFunctionCommand->setOpenGLFunctionsObject(this);
    addSceneCommand(initOpenGLFunctionCommand);
}

cwScene::~cwScene()
{
    delete GeometryItersecter;
}

/**
 * @brief cwScene::paint
 * @param painter
 *
 * This draws the 3d scene using OpenGL
 */
void cwScene::paint()
{
    excuteSceneCommands();

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Simple opaque rendering
    foreach(cwGLObject* item, RenderingObjects) {
        item->draw();
    }

    glDisable(GL_DEPTH_TEST);
}

/**
 * @brief cwScene::addItem
 * @param item
 *
 * Adds the item to be rendered
 */
void cwScene::addItem(cwGLObject *item)
{
    if(!RenderingObjects.contains(item)) {
        RenderingObjects.append(item);
        item->setScene(this);
        update();
    }
}

/**
 * @brief cwScene::removeItem
 * @param item
 *
 * Removes the item to be rendered
 */
void cwScene::removeItem(cwGLObject *item)
{
    if(!RenderingObjects.contains(item)) {
        RenderingObjects.removeOne(item);
        item->setScene(nullptr);
        update();
    }
}

/**
 * @brief cwScene::addSceneCommand
 * @param command
 *
 * Scene commands are subclass and added to a cwScene through this function. Once added the
 * cwScene take responsiblity for command.  All commands are executed
 * in the rendering thread and are useful for initilizing, updating,
 * and deleting opengl resources.
 *
 * Scene all added scene commands are deleted at each rendering frame.
 *
 */
void cwScene::addSceneCommand(cwSceneCommand *command)
{
    Q_ASSERT(!CommandQueue.contains(command));
    CommandQueue.append(command);
    update();
}

/**
 * @brief cwScene::setCamera
 * @param camera
 */
void cwScene::setCamera(cwCamera *camera)
{
    Camera = camera;
}

/**
 * @brief cwScene::camera
 * @return
 */
cwCamera *cwScene::camera() const
{
    return Camera;
}

/**
 * @brief cwScene::geometryItersecter
 * @return
 */
cwGeometryItersecter *cwScene::geometryItersecter() const
{
    return GeometryItersecter;
}

/**
 * @brief cwScene::update
 *
 * This will schedule the scene for rendering
 */
void cwScene::update()
{
    emit needsRendering();
}

/**
 * @brief cwScene::excuteSceneCommands
 *
 * This will excute all the queued scene commands
 */
void cwScene::excuteSceneCommands()
{
    if(!ExcutingCommands) {
        ExcutingCommands = true;
        while(!CommandQueue.isEmpty()) {
            cwSceneCommand* command = CommandQueue.dequeue();
            command->excute();
            delete command;
        }
        ExcutingCommands = false;
    }
}
