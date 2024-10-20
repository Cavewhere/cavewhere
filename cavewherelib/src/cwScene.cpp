/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwScene.h"
#include "cwGLObject.h"
#include "cwRhiItemRenderer.h"
#include "cwSceneCommand.h"
#include "cwShaderDebugger.h"
#include "cwInitializeOpenGLFunctionsCommand.h"
#include "cwDebug.h"

cwScene::cwScene(QObject *parent) :
    QObject(parent),
    GeometryItersecter(new cwGeometryItersecter()),
    ShaderDebugger(new cwShaderDebugger(this)),
    Camera(nullptr),
    ExcutingCommands(false)
{
    // cwInitializeOpenGLFunctionsCommand* initOpenGLFunctionCommand = new cwInitializeOpenGLFunctionsCommand();
    // initOpenGLFunctionCommand->setOpenGLFunctionsObject(this);
    // addSceneCommand(initOpenGLFunctionCommand);
}

cwScene::~cwScene()
{
    delete GeometryItersecter;
    // for(auto command : CommandQueue) {
    //     delete command;
    // }
}

// /**
//  * @brief cwScene::paint
//  * @param painter
//  *
//  * This draws the 3d scene using OpenGL
//  */
// void cwScene::paint()
// {
//     excuteSceneCommands();

//     glClearColor(0.0, 0.0, 0.0, 0.0);
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//     glEnable(GL_DEPTH_TEST);
//     glEnable (GL_BLEND);
//     glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//     //Simple opaque rendering
//     foreach(cwGLObject* item, m_renderingObjects) {
//         item->draw();
//     }

//     glDisable(GL_DEPTH_TEST);
// }

/**
 * @brief cwScene::addItem
 * @param item
 *
 * Adds the item to be rendered
 */
void cwScene::addItem(cwGLObject *item)
{    
    if(m_toDeleteRenderObjects.contains(item)) {
        m_toDeleteRenderObjects.removeOne(item);
    }

    if(!m_renderingObjects.contains(item)) {
        m_newRenderObjects.append(item);
        item->setScene(this);
        item->setParent(this);
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
    if(m_newRenderObjects.contains(item)) {
        m_newRenderObjects.removeOne(item);
    }

    if(!m_renderingObjects.contains(item)) {
        m_toDeleteRenderObjects.append(item);
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
// void cwScene::addSceneCommand(cwSceneCommand *command)
// {
//     Q_ASSERT(!CommandQueue.contains(command));
//     CommandQueue.append(command);
//     update();
// }

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
// void cwScene::excuteSceneCommands()
// {
//     if(!ExcutingCommands) {
//         ExcutingCommands = true;
//         while(!CommandQueue.isEmpty()) {
//             cwSceneCommand* command = CommandQueue.dequeue();
//             command->excute();
//             delete command;
//         }
//         ExcutingCommands = false;
//     }
// }

// void cwScene::checkForGLError(const QByteArray& location)
// {
//     auto error = glGetError();
//     if(error != GL_NO_ERROR) {
//         qDebug() << "GL Error:" << error << location;
//     }
// }

// void cwScene::releaseResources()
// {
//     for(auto object : m_renderingObjects) {
//         object->releaseResources();
//     }
// }

void cwSceneRenderer::initialize()
{

}

void cwSceneRenderer::synchroize(cwScene *scene, cwRhiItemRenderer *renderer)
{
    //Add new rendering object
    if(!scene->m_newRenderObjects.isEmpty()) {
        for(auto object : scene->m_newRenderObjects) {
            auto rhiObject = object->createRHIObject();
            RenderObject renderObject = {object, rhiObject};
            m_rhiObjects.append(renderObject);
            m_rhiObjectsToInitilize.append(renderObject);
        }
        scene->m_newRenderObjects.clear();
    }

    //Remove old rendering objects
    if(!scene->m_toDeleteRenderObjects.isEmpty()) {
        for(auto object : scene->m_toDeleteRenderObjects) {
            auto it = std::remove_if(m_rhiObjects.begin(), m_rhiObjects.end(),
                                     [object](const auto& rhiObject) {
                                         if (rhiObject.glObject == object) {
                                             delete rhiObject.rhiObject;
                                             return true; // Marks for removal
                                         }
                                         return false;
                                     });

            m_rhiObjects.erase(it);
        }
        scene->m_toDeleteRenderObjects.clear();
    }

    //Update rendering objects
    for(auto object : m_rhiObjects) {
        object.rhiObject->synchronize({object.glObject, renderer});
    }

}

void cwSceneRenderer::render(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    auto renderData = cwRHIObject::RenderData({cb, renderer});

    auto rhi = cb->rhi();
    QRhiResourceUpdateBatch* resources = rhi->nextResourceUpdateBatch();
    auto resourceUpdateData = cwRHIObject::ResourceUpdateData({resources, renderData});

    if(!m_rhiObjectsToInitilize.isEmpty()) {
        for(auto object : m_rhiObjectsToInitilize) {
            object.rhiObject->initialize(resourceUpdateData);
        }
        m_rhiObjectsToInitilize.clear();
    }

    if(!m_rhiNeedResourceUpdate.isEmpty()) {
        for(auto object : m_rhiNeedResourceUpdate) {
            object.rhiObject->updateResources(resourceUpdateData);
        }
        m_rhiNeedResourceUpdate.clear();
    }

    //FIXME: This is temporary to test the update
    //Render rendering objects
    for(auto object : m_rhiObjects) {
        object.rhiObject->updateResources(resourceUpdateData);
    }

    //This is a very basic renderer with 1 rendering pass.
    const QColor clearColor = QColor::fromRgbF(0.33, 0.66, 1.0, 1.0);
    cb->beginPass(renderer->renderTarget(), clearColor, { 1.0f, 0 }, resources);

    const QSize outputSize = renderer->renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    //Render rendering objects
    for(auto object : m_rhiObjects) {
        object.rhiObject->render(renderData);
    }

    cb->endPass();
}
