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
#include "cwEDLSettings.h"


//Qt includes
#include <rhi/qrhi.h>

cwScene::cwScene(QObject *parent) :
    QObject(parent),
    GeometryItersecter(new cwGeometryItersecter(this)),
    Camera(nullptr),
    m_edl(new cwEDLSettings(this))
{
    // An EDL tuning change marks the scene dirty and schedules a repaint;
    // cwRhiScene::synchroize() then pulls the new values across to the effect.
    connect(m_edl, &cwEDLSettings::changed, this, &cwScene::update);
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
    // Cancel a pending delete only for the *same* object being re-added. Keyed on
    // the stable id, so a new object reusing a freed address can't false-match a
    // dead entry and steal its cancellation (issue #512).
    m_toDeleteRenderObjects.removeOne(item->renderObjectId());

    m_newRenderObjects.append(item);
    item->setScene(this);
    item->setParent(this);
    update();
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
    const qsizetype removedCount = m_newRenderObjects.removeAll(item)
                                   + (m_toUpdateRenderObjects.remove(item) ? 1 : 0);
    if (removedCount == 0) {
        return;
    }

    // Capture the id while `item` is still alive (the caller deletes it next), so
    // the next sync can drop the matching m_rhiObjectLookup entry without ever
    // dereferencing — or pointer-matching — a dangling pointer.
    const cwRenderObjectId id = item->renderObjectId();
    if (!m_toDeleteRenderObjects.contains(id)) {
        m_toDeleteRenderObjects.append(id);
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
    return m_newRenderObjects.size()
           + m_toUpdateRenderObjects.size();
}


