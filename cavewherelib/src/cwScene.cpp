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
#include "cwDebug.h"
#include "cwRHIObject.h"
#include "cwCamera.h"

//Qt includes
#include <rhi/qrhi.h>

cwScene::cwScene(QObject *parent) :
    QObject(parent),
    GeometryItersecter(new cwGeometryItersecter()),
    Camera(nullptr)
{
}

cwScene::~cwScene()
{
    delete GeometryItersecter;
}

/**
 * @brief cwScene::addItem
 * @param item
 *
 * Adds the item to be rendered
 */
void cwScene::addItem(cwRenderObject *item)
{
    if(m_renderingObjects.contains(item)) {
        return;
    }

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
void cwScene::removeItem(cwRenderObject *item)
{
    if(!m_renderingObjects.contains(item)) {
        return;
    }

    if(m_newRenderObjects.contains(item)) {
        m_newRenderObjects.removeOne(item);
    }

    if(!m_renderingObjects.contains(item)) {
        m_toDeleteRenderObjects.append(item);
        item->setScene(nullptr);
        update();
    }
}

void cwScene::updateItem(cwRenderObject *item)
{
    m_toUpdateRenderObjects.insert(item);
    update();
}

/**
 * @brief cwScene::setCamera
 * @param camera
 */
void cwScene::setCamera(cwCamera *camera)
{
    if(Camera != camera) {

        if(Camera != nullptr) {
            Camera->disconnect(this);
        }

        Camera = camera;

        if(Camera != nullptr) {
            auto updateFlag = [this](cwSceneUpdate::Flag flag) {
                m_updateFlags |= flag;
                update();
            };

            connect(Camera, &cwCamera::devicePixelRatioChanged, this, [updateFlag]() {
                updateFlag(cwSceneUpdate::Flag::DevicePixelRatio);
            });
            connect(Camera, &cwCamera::viewMatrixChanged, this, [updateFlag]() {
                updateFlag(cwSceneUpdate::Flag::ViewMatrix);
            });
            connect(Camera, &cwCamera::projectionChanged, this, [updateFlag]() {
                updateFlag(cwSceneUpdate::Flag::ProjectionMatrix);
            });

            m_updateFlags = cwSceneUpdate::Flag::DevicePixelRatio
                            | cwSceneUpdate::Flag::ProjectionMatrix
                            | cwSceneUpdate::Flag::ViewMatrix;
        }

        emit cameraChanged();
    }
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

void cwSceneRenderer::initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    //This function is called multiple times, make sure bufferes that are initilized
    //here are done so only once, or shaders will not update correctly.

    if(m_globalUniformBuffer == nullptr) {
        auto rhi = cb->rhi();
        auto size = rhi->ubufAligned(sizeof(GlobalUniform)); //Makes the uniform buffer size portable
        m_globalUniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
        m_globalUniformBuffer->create();
    }
}

void cwSceneRenderer::synchroize(cwScene *scene, cwRhiItemRenderer *renderer)
{
    m_updateFlags = scene->m_updateFlags;
    scene->m_updateFlags = cwSceneUpdate::Flag::None;

    //Update camera uniform buffer
    if(scene->camera()) {

        if(needsUpdate(cwSceneUpdate::Flag::DevicePixelRatio)) {
            m_devicePixelRatio = scene->camera()->devicePixelRatio();
        }

        if(needsUpdate(cwSceneUpdate::Flag::ViewMatrix)) {
            m_viewMatrix = scene->camera()->viewMatrix();
        }

        if(needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix)) {
            m_projectionMatrix = scene->camera()->projectionMatrix();
        }
    }

    //Add new rendering object
    if(!scene->m_newRenderObjects.isEmpty()) {
        for(auto object : scene->m_newRenderObjects) {
            auto rhiObject = object->createRHIObject();
            qDebug() << "Creating rhiObject:" << object << "->" << rhiObject;
            m_rhiObjects.append(rhiObject);
            m_rhiObjectsToInitilize.append(rhiObject);
            m_rhiObjectLookup[object] = rhiObject;

            scene->m_toUpdateRenderObjects.insert(object);
        }
        scene->m_newRenderObjects.clear();
    }

    //Remove old rendering objects
    if(!scene->m_toDeleteRenderObjects.isEmpty()) {
        for(auto object : scene->m_toDeleteRenderObjects) {
            auto rhiObject = m_rhiObjectLookup[object];
            if(rhiObject != nullptr) {
                auto it = std::remove_if(m_rhiObjects.begin(), m_rhiObjects.end(),
                                         [rhiObject](const cwRHIObject* currentRhiObject) {
                                             if (currentRhiObject == rhiObject) {
                                                 //FIXME: schedule for release resources
                                                 delete currentRhiObject;
                                                 return true; // Marks for removal
                                             }
                                             return false;
                                         });
                m_rhiObjects.erase(it);
                m_rhiObjectLookup.remove(object);
            }

        }
        scene->m_toDeleteRenderObjects.clear();
    }

    //Update rendering objects
    for(auto object : std::as_const(scene->m_toUpdateRenderObjects)) {
        auto rhiObject = m_rhiObjectLookup[object];
        if(rhiObject) {
            rhiObject->synchronize({object, renderer});
            m_rhiNeedResourceUpdate.append(rhiObject);
        }
    }
    scene->m_toUpdateRenderObjects.clear();

}

void cwSceneRenderer::render(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    auto renderData = cwRHIObject::RenderData({cb, renderer, m_updateFlags});

    auto rhi = cb->rhi();
    QRhiResourceUpdateBatch* resources = rhi->nextResourceUpdateBatch();
    auto resourceUpdateData = cwRHIObject::ResourceUpdateData({resources, renderData});

    //Upate uniforms for all the objects
    updateGlobalUniformBuffer(resources, rhi);

    if(!m_rhiObjectsToInitilize.isEmpty()) {
        for(auto object : m_rhiObjectsToInitilize) {
            object->initialize(resourceUpdateData);
        }
        m_rhiObjectsToInitilize.clear();
    }

    if(!m_rhiNeedResourceUpdate.isEmpty()) {
        for(auto object : m_rhiNeedResourceUpdate) {
            object->updateResources(resourceUpdateData);
        }
        m_rhiNeedResourceUpdate.clear();
    }

    //This is a very basic renderer with 1 rendering pass.
    const QColor clearColor = QColor::fromRgbF(0.33, 0.66, 1.0, 1.0);
    cb->beginPass(renderer->renderTarget(), clearColor, { 1.0f, 0 }, resources);

    const QSize outputSize = renderer->renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    //Render rendering objects
    for(auto object : m_rhiObjects) {
        object->render(renderData);
    }

    cb->endPass();
}

void cwSceneRenderer::updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi)
{
    if(needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix) || needsUpdate(cwSceneUpdate::Flag::ViewMatrix)) {
        m_projectionCorrectedMatrix = rhi->clipSpaceCorrMatrix() * m_projectionMatrix;
        m_viewProjectionMatrix = m_projectionCorrectedMatrix * m_viewMatrix;

        batch->updateDynamicBuffer(
            m_globalUniformBuffer,
            offsetof(GlobalUniform, viewProjectionMatrix),
            sizeof(GlobalUniform::viewProjectionMatrix),
            m_viewProjectionMatrix.constData()
            );


        if(needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix)) {
            batch->updateDynamicBuffer(
                m_globalUniformBuffer,
                offsetof(GlobalUniform, projectionMatrix),
                sizeof(GlobalUniform::projectionMatrix),
                m_projectionCorrectedMatrix.constData()
                );
        }

        if(needsUpdate(cwSceneUpdate::Flag::ViewMatrix)) {
            batch->updateDynamicBuffer(
                m_globalUniformBuffer,
                offsetof(GlobalUniform, viewMatrix),
                sizeof(GlobalUniform::viewMatrix),
                m_viewMatrix.constData()
                );
        }
    }

    m_updateFlags = cwSceneUpdate::Flag::None;
}

QString cwSceneUpdate::flagToString(Flag flag) {
    QStringList flagNames;
    if (flag == Flag::None) {
        flagNames << "None";
    } else {
        if (isFlagSet(flag, Flag::ViewMatrix)) {
            flagNames << "ViewMatrix";
        }
        if (isFlagSet(flag, Flag::ProjectionMatrix)) {
            flagNames << "ProjectionMatrix";
        }
        if (isFlagSet(flag, Flag::DevicePixelRatio)) {
            flagNames << "DevicePixelRatio";
        }
    }
    return flagNames.join(" | ");
}
