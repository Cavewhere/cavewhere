/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwScene.h"
#include "cwRenderObject.h"
#include "cwDebug.h"
#include "cwRHIObject.h"
#include "cwCamera.h"
#include "cwGeometryItersecter.h"


//Qt includes
#include <rhi/qrhi.h>

cwScene::cwScene(QObject *parent) :
    QObject(parent),
    GeometryItersecter(new cwGeometryItersecter(this)),
    Camera(nullptr)
{
}

cwScene::~cwScene() = default;

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
    if (item == nullptr) {
        return;
    }

    // Drop live references before the caller deletes `item` (issue #491);
    // synchroize() dereferences entries in m_newRenderObjects and
    // m_toUpdateRenderObjects.
    const qsizetype removedCount = m_renderingObjects.removeAll(item)
                                   + m_newRenderObjects.removeAll(item)
                                   + (m_toUpdateRenderObjects.remove(item) ? 1 : 0);
    if (removedCount == 0) {
        return;
    }

    // Pointer used as hash key only (never dereferenced) so the next sync
    // drops the matching m_rhiObjectLookup entry.
    if (!m_toDeleteRenderObjects.contains(item)) {
        m_toDeleteRenderObjects.append(item);
    }
    update();
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

int cwScene::pendingItemCount() const
{
    return m_renderingObjects.size()
           + m_newRenderObjects.size()
           + m_toUpdateRenderObjects.size();
}


